// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/*
  EmissionMonitoring.sol - production-oriented contract for vehicle emission PUC endorsements.

  Notes:
  - Emission units:
      * CO, NOx, HC are integers scaled by SCALE (e.g., SCALE = 1e6). Edge node must send integer.
      * CO2 is integer (ppm) — you can change to scaled form if desired.
  - Endorsements require an ECDSA signature produced by the region's MVI off-chain.
  - Reward distribution is via an ERC20 token address set by owner. The contract must be approved
    to transfer tokens on behalf of the reward distributor OR the contract itself should hold tokens.
  - ElGamal: not implemented on-chain. Use off-chain ElGamal and provide an ECDSA attestation if needed.
*/

interface IERC20 {
    function transfer(address recipient, uint256 amount) external returns (bool);
}

contract EmissionMonitoring {

    // ------------------------
    // Ownership / Administration
    // ------------------------
    address public owner;
    bool public paused;

    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner");
        _;
    }

    modifier whenNotPaused() {
        require(!paused, "Paused");
        _;
    }

    event OwnershipTransferred(address indexed previousOwner, address indexed newOwner);
    event Paused(address indexed by);
    event Unpaused(address indexed by);

    // ------------------------
    // Types and constants
    // ------------------------
    enum FuelType { Petrol, Diesel, CNG }
    enum Status { Pending, Compliant, NonCompliant } // Pending = not yet endorsed

    // Scale for fractional sensor values (e.g., CO 0.05 -> send 0.05 * SCALE)
    uint256 public constant SCALE = 1e6;

    struct Vehicle {
        address owner;
        FuelType fuelType;
        string region;
        bool registered;
        bool restricted;
    }

    struct EmissionRecord {
        uint256 timestamp;     // block.timestamp when submitted
        uint256 CO;            // scaled by SCALE (integer)
        uint256 NOx;           // scaled by SCALE (integer)
        uint256 HC;            // scaled by SCALE (integer)
        uint256 CO2;           // integer (ppm) or scaled value
        Status status;
        bool endorsed;
        address reporter;      // edge-node/submitter address
        bytes32 dataHash;      // keccak256 of the recorded values (for signature verification)
    }

    struct MVI {
        address mviAddress;
        bool exists;
    }

    // ------------------------
    // State
    // ------------------------
    // vehicleId -> Vehicle
    mapping(string => Vehicle) public vehicles;
    // vehicleId -> emission records array
    mapping(string => EmissionRecord[]) private vehicleRecords;
    // region -> MVI
    mapping(string => MVI) public regionMVI;
    // reward token & amount (owner can fund the contract)
    IERC20 public rewardToken;
    uint256 public rewardAmount; // token units
    // thresholds per fuel type (CO, NOx, HC, CO2)
    struct Thresholds { uint256 CO; uint256 NOx; uint256 HC; uint256 CO2; }
    mapping(FuelType => Thresholds) public thresholds;

    // replay protection: track used data hashes (or record-level nonces)
    mapping(bytes32 => bool) public usedSignatures;

    // events
    event VehicleRegistered(address indexed user, string vehicleId, FuelType fuelType, string region);
    event VehicleUnregistered(string vehicleId);
    event EmissionSubmitted(string indexed vehicleId, uint256 recordIndex, address indexed reporter);
    event EndorsementApproved(string indexed vehicleId, uint256 recordIndex, address indexed mvi);
    event EndorsementRejected(string indexed vehicleId, uint256 recordIndex, string reason);
    event VehicleFlagged(string vehicleId);
    event VehicleRestricted(string vehicleId);
    event VehicleUnrestricted(string vehicleId);
    event MVISet(string region, address mvi);
    event ThresholdsUpdated(FuelType fuelType, uint256 CO, uint256 NOx, uint256 HC, uint256 CO2);
    event RewardConfigUpdated(address token, uint256 amount);
    event TokensTransferred(address indexed to, uint256 amount);

    // ------------------------
    // Constructor
    // ------------------------
    constructor() {
        owner = msg.sender;
        paused = false;

        // set default thresholds (assume scaled CO/NOx/HC in SCALE units, CO2 in ppm)
        thresholds[FuelType.Diesel] = Thresholds({CO: SCALE, NOx: SCALE / 2, HC: SCALE / 20, CO2: 1500});
        thresholds[FuelType.Petrol] = Thresholds({CO: SCALE / 2, NOx: SCALE / 4, HC: SCALE * 3 / 100, CO2: 1200});
        thresholds[FuelType.CNG] = Thresholds({CO: SCALE * 3 / 10, NOx: SCALE * 15 / 100, HC: SCALE / 50, CO2: 1000});
        emit OwnershipTransferred(address(0), owner);
    }

    // ------------------------
    // Admin/owner functions
    // ------------------------
    function transferOwnership(address newOwner) external onlyOwner {
        require(newOwner != address(0), "zero address");
        emit OwnershipTransferred(owner, newOwner);
        owner = newOwner;
    }

    function pause() external onlyOwner {
        paused = true;
        emit Paused(msg.sender);
    }

    function unpause() external onlyOwner {
        paused = false;
        emit Unpaused(msg.sender);
    }

    function setRewardToken(address tokenAddr, uint256 amount) external onlyOwner {
        require(tokenAddr != address(0), "zero token");
        rewardToken = IERC20(tokenAddr);
        rewardAmount = amount;
        emit RewardConfigUpdated(tokenAddr, amount);
    }

    function setRewardAmount(uint256 amount) external onlyOwner {
        rewardAmount = amount;
        emit RewardConfigUpdated(address(rewardToken), amount);
    }

    function setMVI(string calldata region, address mviAddress) external onlyOwner {
        require(mviAddress != address(0), "zero address");
        regionMVI[region] = MVI({ mviAddress: mviAddress, exists: true });
        emit MVISet(region, mviAddress);
    }

    function clearMVI(string calldata region) external onlyOwner {
        delete regionMVI[region];
        emit MVISet(region, address(0));
    }

    function setThresholds(FuelType fuelType, uint256 CO_scaled, uint256 NOx_scaled, uint256 HC_scaled, uint256 CO2) external onlyOwner {
        thresholds[fuelType] = Thresholds({CO: CO_scaled, NOx: NOx_scaled, HC: HC_scaled, CO2: CO2});
        emit ThresholdsUpdated(fuelType, CO_scaled, NOx_scaled, HC_scaled, CO2);
    }

    // ------------------------
    // Vehicle lifecycle
    // ------------------------
    function registerVehicle(string calldata vehicleId, FuelType fuelType, string calldata region) external whenNotPaused {
        require(!vehicles[vehicleId].registered, "Already registered");
        vehicles[vehicleId] = Vehicle({ owner: msg.sender, fuelType: fuelType, region: region, registered: true, restricted: false });
        emit VehicleRegistered(msg.sender, vehicleId, fuelType, region);
    }

    function unregisterVehicle(string calldata vehicleId) external {
        require(vehicles[vehicleId].registered, "Not registered");
        require(vehicles[vehicleId].owner == msg.sender || msg.sender == owner, "Not authorized");
        delete vehicles[vehicleId];
        delete vehicleRecords[vehicleId];
        emit VehicleUnregistered(vehicleId);
    }

    function restrictVehicle(string calldata vehicleId) external onlyOwner {
        require(vehicles[vehicleId].registered, "Not registered");
        vehicles[vehicleId].restricted = true;
        emit VehicleRestricted(vehicleId);
    }

    function liftRestriction(string calldata vehicleId) external onlyOwner {
        require(vehicles[vehicleId].registered, "Not registered");
        vehicles[vehicleId].restricted = false;
        emit VehicleUnrestricted(vehicleId);
    }

    // ------------------------
    // Emission submission: data should be pre-scaled for fractional fields
    // ------------------------
    function submitEmission(
        string calldata vehicleId,
        uint256 CO_scaled,
        uint256 NOx_scaled,
        uint256 HC_scaled,
        uint256 CO2_ppm
    ) external whenNotPaused {
        Vehicle memory v = vehicles[vehicleId];
        require(v.registered, "Vehicle not registered");
        require(!v.restricted, "Vehicle restricted");

        // Compute status based on thresholds (local, initial check)
        Status localStatus = _validateEmission(v.fuelType, CO_scaled, NOx_scaled, HC_scaled, CO2_ppm);

        // Data hash for signature / replay protection
        bytes32 dataHash = keccak256(abi.encodePacked(vehicleId, block.timestamp, CO_scaled, NOx_scaled, HC_scaled, CO2_ppm, msg.sender));

        EmissionRecord memory rec = EmissionRecord({
            timestamp: block.timestamp,
            CO: CO_scaled,
            NOx: NOx_scaled,
            HC: HC_scaled,
            CO2: CO2_ppm,
            status: localStatus,
            endorsed: false,
            reporter: msg.sender,
            dataHash: dataHash
        });

        vehicleRecords[vehicleId].push(rec);

        uint256 index = vehicleRecords[vehicleId].length - 1;
        emit EmissionSubmitted(vehicleId, index, msg.sender);

        // If local check is NonCompliant, automatically flag & optionally restrict (owner decision)
        if (localStatus == Status.NonCompliant) {
            emit VehicleFlagged(vehicleId);
        }
    }

    // ------------------------
    // Endorsement flow (MVI)
    // - The MVI for the region must sign off-chain the specific data hash and supply signature components.
    // - We verify ECDSA signature using ecrecover. This is common on-chain pattern.
    // ------------------------
    function endorseRecord(
        string calldata vehicleId,
        uint256 recordIndex,
        uint8 v,
        bytes32 r,
        bytes32 s
    ) external whenNotPaused {
        require(vehicles[vehicleId].registered, "Vehicle not registered");
        require(recordIndex < vehicleRecords[vehicleId].length, "Invalid record index");

        EmissionRecord storage rec = vehicleRecords[vehicleId][recordIndex];
        require(!rec.endorsed, "Already endorsed");

        // Look up MVI for vehicle's region
        MVI memory m = regionMVI[vehicles[vehicleId].region];
        require(m.exists, "No MVI for region");
        // Recreate prefixed message hash (EIP-191 / eth_sign style)
        bytes32 ethSignedHash = _getEthSignedMessageHash(rec.dataHash);

        // Replay protection
        require(!usedSignatures[ethSignedHash], "Signature already used");

        // Recover signer
        address signer = ecrecover(ethSignedHash, v, r, s);
        require(signer == m.mviAddress, "Invalid signature: not region MVI");

        // Mark used
        usedSignatures[ethSignedHash] = true;
        rec.endorsed = true;

        // If record was compliant, mint/transfer reward tokens to vehicle owner
        if (rec.status == Status.Compliant && address(rewardToken) != address(0) && rewardAmount > 0) {
            // contract should hold tokens or owner should have pre-funded it
            bool ok = rewardToken.transfer(vehicles[vehicleId].owner, rewardAmount);
            if (ok) {
                emit TokensTransferred(vehicles[vehicleId].owner, rewardAmount);
            }
        }

        emit EndorsementApproved(vehicleId, recordIndex, msg.sender);
    }

    // Batch endorse (single MVI endorses multiple records) - gas heavy, use carefully
    function batchEndorse(
        string[] calldata vehicleIds,
        uint256[] calldata recordIndices,
        uint8[] calldata vs,
        bytes32[] calldata rs,
        bytes32[] calldata ss
    ) external whenNotPaused {
        require(vehicleIds.length == recordIndices.length, "Length mismatch");
        require(vehicleIds.length == vs.length && vs.length == rs.length && rs.length == ss.length, "Sig arrays mismatch");

        for (uint256 i = 0; i < vehicleIds.length; ++i) {
            // Call endorseRecord but avoid duplicate checks overhead: replicate logic inline
            string calldata vid = vehicleIds[i];
            uint256 idx = recordIndices[i];
            require(vehicles[vid].registered, "Vehicle not registered");
            require(idx < vehicleRecords[vid].length, "Invalid record index");

            EmissionRecord storage rec = vehicleRecords[vid][idx];
            require(!rec.endorsed, "Already endorsed");

            MVI memory m = regionMVI[vehicles[vid].region];
            require(m.exists, "No MVI for region");

            bytes32 ethSignedHash = _getEthSignedMessageHash(rec.dataHash);
            require(!usedSignatures[ethSignedHash], "Signature used");

            address signer = ecrecover(ethSignedHash, vs[i], rs[i], ss[i]);
            require(signer == m.mviAddress, "Invalid signature");

            usedSignatures[ethSignedHash] = true;
            rec.endorsed = true;

            if (rec.status == Status.Compliant && address(rewardToken) != address(0) && rewardAmount > 0) {
                bool ok = rewardToken.transfer(vehicles[vid].owner, rewardAmount);
                if (ok) {
                    emit TokensTransferred(vehicles[vid].owner, rewardAmount);
                }
            }

            emit EndorsementApproved(vid, idx, msg.sender);
        }
    }

    // ------------------------
    // Utilities & getters
    // ------------------------
    function _validateEmission(FuelType fuelType, uint256 CO_scaled, uint256 NOx_scaled, uint256 HC_scaled, uint256 CO2_ppm) internal view returns (Status) {
        Thresholds memory t = thresholds[fuelType];
        if (CO_scaled > t.CO || NOx_scaled > t.NOx || HC_scaled > t.HC || CO2_ppm > t.CO2) {
            return Status.NonCompliant;
        }
        return Status.Compliant;
    }

    function getRecordsCount(string calldata vehicleId) external view returns (uint256) {
        return vehicleRecords[vehicleId].length;
    }

    function getRecord(string calldata vehicleId, uint256 index) external view returns (
        uint256 timestamp,
        uint256 CO_scaled,
        uint256 NOx_scaled,
        uint256 HC_scaled,
        uint256 CO2_ppm,
        Status status,
        bool endorsed,
        address reporter
    ) {
        require(index < vehicleRecords[vehicleId].length, "Index out of range");
        EmissionRecord memory r = vehicleRecords[vehicleId][index];
        return (r.timestamp, r.CO, r.NOx, r.HC, r.CO2, r.status, r.endorsed, r.reporter);
    }

    // helper: create msg prefix to match eth_sign signing used by many wallets
    function _getEthSignedMessageHash(bytes32 _messageHash) internal pure returns (bytes32) {
        // "\x19Ethereum Signed Message:\n32" + messageHash
        return keccak256(abi.encodePacked("\x19Ethereum Signed Message:\n32", _messageHash));
    }

    // retrieve the stored dataHash for a record (useful for off-chain verification)
    function getRecordDataHash(string calldata vehicleId, uint256 index) external view returns (bytes32) {
        require(index < vehicleRecords[vehicleId].length, "Index out of range");
        return vehicleRecords[vehicleId][index].dataHash;
    }

    // ------------------------
    // Signature helpers for off-chain use
    // ------------------------
    // Off-chain flow:
    // 1) Node submits emission -> rec.dataHash stored and returned (call getRecordDataHash)
    // 2) MVI signs rec.dataHash off-chain with their private key (eth_sign style)
    // 3) Submit v,r,s to endorseRecord() to mark endorsed on-chain
    //
    // Example: signMessage = web3.eth.accounts.sign(dataHashHex, mviPrivateKey)

    // ------------------------
    // Safety / fallback: owner can rescue ERC20 tokens sent to contract
    // ------------------------
    function rescueERC20(address tokenAddr, address to, uint256 amount) external onlyOwner {
        require(to != address(0), "zero address");
        IERC20(tokenAddr).transfer(to, amount);
    }
}
