# PUCC - Pollution Under Control Certificate System

A comprehensive blockchain-based vehicle emission monitoring and certification system that combines network simulation, data processing, smart contracts, and real-time monitoring capabilities.

## 🚀 Project Overview

This project implements a decentralized Pollution Under Control Certificate (PUC) system using:

- **Blockchain technology** for transparent and immutable emission records
- **Network simulation** for testing vehicular communication protocols
- **Real-time data filtering** for noise reduction in emission measurements
- **Machine Learning** for predictive analytics and anomaly detection
- **Smart contracts** for automated compliance verification and rewards

## 📁 Project Structure

```
PUCC/
├── README.md                    # Project documentation and usage instructions
├── LICENSE                      # MIT License for the project
├── .gitignore                   # Git ignore rules for all project components
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
├── xgboost/                     # Machine Learning and Predictive Analytics
│   └── predictive_engine.py     # XGBoost-based emission prediction and anomaly detection
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

### 🤖 Machine Learning Engine (`xgboost/`)

- **Purpose**: Predictive analytics and intelligent emission monitoring
- **Tech Stack**: Python, XGBoost, Scikit-learn, Pandas, NumPy
- **Component**: `predictive_engine.py`
- **Key Features**:
  - **Predictive Modeling**: XGBoost regression for CO2 emission prediction
  - **Hyperparameter Optimization**: Grid search with 5-fold cross-validation
  - **Anomaly Detection**: Isolation Forest for identifying unusual emission patterns
  - **Incremental Learning**: Continuous model updates with new data
  - **Maintenance Prediction**: Decision tree for estimating service intervals
  - **Real-time Processing**: Processes records every 15 minutes
  - **Multi-fuel Support**: Handles Petrol, Diesel, and CNG vehicles
- **ML Pipeline**:
  - Data preprocessing and feature engineering
  - Automated model training and validation
  - Real-time inference and decision making
  - Model persistence and incremental updates

## 🏗️ System Architecture

```
                    ┌─────────────────────────────────────────────────────────┐
                    │                 PUCC ECOSYSTEM                          │
                    └─────────────────────────────────────────────────────────┘
                                                │
          ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
          │   Vehicle       │───▶│  Edge Devices   │───▶│  Noise Filter   │
          │   Sensors       │    │  (Data Sim)     │    │  (Backend)      │
          └─────────────────┘    └─────────────────┘    └─────────┬───────┘
              ▲                                                   │
              │                                                   ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   NS-3          │◀───│   Blockchain    │◀───│   Data          │───▶│   ML Engine     │
│   Network Sim   │    │   (Smart        │    │   Validation    │    │   (XGBoost)     │
└─────────────────┘    │   Contracts)    │    └─────────────────┘    └─────────┬───────┘
                       └─────────────────┘                                      │
                                 ▲                                              │
                                 │                                              ▼
                       ┌─────────────────┐                          ┌─────────────────┐
                       │      MVI        │                          │   Predictive    │
                       │  Endorsement    │                          │   Analytics     │
                       └─────────────────┘                          │   • Anomaly     │
                                                                    │   • Compliance  │
                                                                    │   • Maintenance │
                                                                    └─────────────────┘

Data Flow:
1. Vehicle sensors → Edge simulation → Noise filtering
2. Filtered data → ML analysis → Predictive insights
3. Validated data → Blockchain storage → Smart contract verification
4. MVI endorsement → Token rewards → Network simulation validation
```

## 🚀 Getting Started

### Prerequisites

- Node.js (v16 or higher)
- Python 3.8+ with pip
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

6. **Set up Machine Learning Engine**

   ```bash
   cd xgboost
   # Install Python dependencies
   pip install xgboost scikit-learn pandas numpy joblib

   # Train the model
   python train_model.py --dataset vca_uk_emissions.csv

   # Simulate real-time processing
   python predictive_engine.py --train_csv path/to/your/dataset.csv --simulate_realtime
   ```

7. **Set up Machine Learning Engine**

   ```bash
   npm install
   node sensor-bridge.js --sensors=co,nox,pm
   ```

## 📊 Features

### ✅ Implemented

- Real-time emission data simulation
- Noise filtering algorithms for sensor data
- Express.js API for data processing
- Comprehensive smart contract for emission monitoring
- Network simulation framework setup
- **XGBoost-based predictive analytics engine**
- **Anomaly detection with Isolation Forest**
- **Real-time emission monitoring and prediction**
- **Maintenance interval prediction**

### 🚧 In Development

- Integration between all components
- Web dashboard for monitoring
- Mobile application for vehicle owners
- Advanced ML model optimization
- Real-time data pipeline integration

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
5. **Service Centers**: Receive predictive maintenance alerts and recommendations
6. **Insurance Companies**: Access verified emission data for premium calculations

## 🧠 Machine Learning Features

### **Predictive Analytics**

- **CO2 Emission Prediction**: Accurate forecasting based on vehicle characteristics
- **Compliance Forecasting**: Predicts future compliance status
- **Maintenance Scheduling**: Intelligent service interval recommendations

### **Anomaly Detection**

- **Real-time Monitoring**: Identifies unusual emission patterns
- **Fraud Prevention**: Detects potentially manipulated sensor data
- **Quality Assurance**: Flags readings requiring additional verification

### **Continuous Learning**

- **Incremental Updates**: Models improve with new data
- **Cross-validation**: Robust model validation with 5-fold CV
- **Hyperparameter Optimization**: Automated parameter tuning

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
