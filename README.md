# PUCC - Pollution Under Control Certificate System

A comprehensive blockchain-based vehicle emission monitoring and certification system that combines network simulation, data processing, smart contracts, and real-time monitoring capabilities.

## 🚀 Project Overview

This project implements a decentralized Pollution Under Control Certificate (PUC) system using:

- **Blockchain technology** for transparent and immutable emission records
- **Network simulation** for testing vehicular communication protocols
- **Real-time data filtering** for noise reduction in emission measurements
- **Smart contracts** for automated compliance verification and rewards

## 📁 Project Structure

```
PUCC/
├── README.md                    # Project documentation and usage instructions
├── data_simulation/             # Vehicle emission data simulation components
│   ├── emission_client          # Compiled emission data client
│   └── emission_client.cpp      # C++ source for emission data generation
├── noise_filtering/             # Backend server for data processing
│   ├── package.json             # Node.js dependencies and scripts
│   ├── package-lock.json        # Exact versions of installed dependencies
│   ├── server.js                # Express.js server for emission data processing
│   ├── emissions.log            # Log file for recorded emission data
│   ├── node_modules/            # Installed Node.js dependencies
│   └── utils/                   # Utility functions for data processing
│       ├── logger.js            # Logging utilities for system events
│       └── noiseFilter.js       # Noise filtering algorithms for sensor data
├── ns-3/                        # Network simulation components
│   └── scratch/                 # NS-3 simulation scripts
│       ├── vehicle-sim.cc       # Vehicle network communication simulation
│       └── ns-sim.cpp           # Basic network simulation (needs correction)
├── solidity/                    # Blockchain smart contracts
│   └── EmissionMonitoring.sol   # Main smart contract for emission monitoring
└── .vscode/                     # VS Code configuration files
```

## 🔧 Component Details

### 📊 Data Simulation (`data_simulation/`)

- **Purpose**: Generates realistic vehicle emission data for testing
- **Components**:
  - `emission_client.cpp`: C++ application that simulates emission sensors
  - `emission_client`: Compiled executable for data generation
- **Usage**: Simulates CO, NOx, HC, and CO2 emission levels for different vehicle types

### 🌐 Noise Filtering (`noise_filtering/`)

- **Purpose**: Backend server for processing and filtering emission data
- **Tech Stack**: Node.js, Express.js, Zod validation, Chalk logging
- **Components**:
  - `server.js`: Main Express server handling API requests
  - `utils/logger.js`: Centralized logging system with different log levels
  - `utils/noiseFilter.js`: Advanced algorithms for filtering sensor noise
  - `emissions.log`: Persistent storage for emission measurements
- **Dependencies**:
  - `express ^5.1.0`: Web framework for API endpoints
  - `chalk ^5.5.0`: Terminal styling for enhanced logging
  - `zod ^4.0.16`: Schema validation for incoming data

### 🖧 Network Simulation (`ns-3/`)

- **Purpose**: Simulates vehicle-to-infrastructure (V2I) communication
- **Framework**: NS-3 network simulator
- **Components**:
  - `vehicle-sim.cc`: Complete vehicular network simulation
  - `ns-sim.cpp`: Basic point-to-point communication simulation
- **Features**: Tests network protocols, latency, and data transmission reliability

### ⛓️ Smart Contracts (`solidity/`)

- **Purpose**: Blockchain-based emission monitoring and certification
- **Language**: Solidity ^0.8.20
- **Contract**: `EmissionMonitoring.sol`
- **Key Features**:
  - Vehicle registration and management
  - Emission data submission with timestamp validation
  - Multi-signature endorsement by Motor Vehicle Inspectors (MVI)
  - Automatic compliance checking against fuel-specific thresholds
  - ERC20 token rewards for compliant vehicles
  - Fraud prevention with ECDSA signature verification
  - Administrative controls and emergency pause functionality

## 🏗️ System Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Vehicle       │───▶│  Edge Devices   │───▶│  Noise Filter   │
│   Sensors       │    │  (Data Sim)     │    │  (Backend)      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                                        │
                                                        ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   NS-3          │    │   Blockchain    │◀───│   Data          │
│   Network Sim   │    │   (Smart        │    │   Validation    │
└─────────────────┘    │   Contracts)    │    └─────────────────┘
                       └─────────────────┘
```

## 🚀 Getting Started

### Prerequisites

- Node.js (v16 or higher)
- GCC compiler for C++ components
- NS-3 simulator (for network simulation)
- Solidity development environment (Hardhat/Truffle)

### Installation

1. **Clone the repository**

   ```bash
   git clone <repository-url>
   cd PUCC
   ```

2. **Set up the backend server**

   ```bash
   cd noise_filtering
   npm install
   npm start
   ```

3. **Compile data simulation**

   ```bash
   cd data_simulation
   g++ -o emission_client emission_client.cpp
   ./emission_client
   ```

4. **Build NS-3 simulations**

   ```bash
   cd ns-3
   # Configure and build NS-3 environment
   ./waf configure --enable-examples
   ./waf build
   ./waf --run scratch/vehicle-sim
   ```

5. **Deploy smart contracts**
   ```bash
   cd solidity
   # Deploy using your preferred framework
   npx hardhat compile
   npx hardhat deploy
   ```

## 📊 Features

### ✅ Implemented

- Real-time emission data simulation
- Noise filtering algorithms for sensor data
- Express.js API for data processing
- Comprehensive smart contract for emission monitoring
- Network simulation framework setup

### 🚧 In Development

- Integration between all components
- Web dashboard for monitoring
- Mobile application for vehicle owners
- Advanced ML-based noise filtering

## 🔐 Security Features

- **ECDSA Signature Verification**: Prevents unauthorized endorsements
- **Replay Attack Protection**: Ensures signatures cannot be reused
- **Access Control**: Role-based permissions for different system actors
- **Emergency Controls**: Owner can pause system during emergencies
- **Data Integrity**: Cryptographic hashing of emission records

## 🎯 Use Cases

1. **Vehicle Owners**: Submit emission data and receive digital certificates
2. **Regulatory Authorities**: Monitor compliance and issue endorsements
3. **Environmental Agencies**: Track pollution trends and enforce standards
4. **Research Institutions**: Analyze emission patterns and effectiveness

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/nikil-sri-shen/Vehicle-Emission-Monitoring-system/blob/main/LICENSE) file for details.

## 📞 Support

For questions and support, please open an issue in the repository or contact the development team.
