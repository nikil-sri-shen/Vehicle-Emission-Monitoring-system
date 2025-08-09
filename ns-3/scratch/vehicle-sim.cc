// scratch/vehicle_emission_sim.cc
// Build: copy to ns-3/scratch/ and run via: ./ns3 run scratch/vehicle_emission_sim --NumVehicles=150 --SimTime=120 --PktInterval=5

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include <random>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicleEmissionSim");

/* Simple enum for fuel types */
enum FuelType { PETROL=0, DIESEL=1, CNG=2 };

/* Configurable parameters via CLI */
static uint32_t g_numVehicles = 100;
static double g_simTime = 120.0;    // seconds
static double g_pktInterval = 5.0;  // seconds between emission sends per vehicle
static uint32_t g_filterWindow = 5; // moving average window
static std::string g_outputCsv = "emission_records.csv";
static bool g_enablePcap = false;
static bool g_enableFlowMonitor = true;

/* Utility: generate ISO timestamp-like string for CSV (sim time) */
static std::string SimTimeString(Ptr<Simulation> s, Time t) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3) << t.GetSeconds();
    return ss.str();
}

/* Helper: Per-vehicle emission model */
class VehicleEmissionModel {
public:
    VehicleEmissionModel(uint32_t id, FuelType fuelType) :
        m_id(id), m_fuel(fuelType), m_rng(m_rd()), m_norm(0.0, 1.0), m_spikeDist(0.0, 1.0)
    {
        // baseline values (means) — these are realistic-ish placeholders
        if (m_fuel == DIESEL)       { m_baseCO = 1.2; m_baseNOx = 0.55; m_baseHC = 0.04; m_baseCO2 = 1500; }
        else if (m_fuel == PETROL)  { m_baseCO = 0.6; m_baseNOx = 0.25; m_baseHC = 0.03; m_baseCO2 = 1200; }
        else /* CNG */              { m_baseCO = 0.35; m_baseNOx = 0.15; m_baseHC = 0.02; m_baseCO2 = 1000; }

        // sensor noise standard deviations (fractional)
        m_sigmaCO  = 0.05 * m_baseCO;   // 5% noise
        m_sigmaNOx = 0.05 * m_baseNOx;
        m_sigmaHC  = 0.05 * m_baseHC;
        m_sigmaCO2 = 15.0; // ppm noise
    }

    struct Sample {
        double CO;
        double NOx;
        double HC;
        uint32_t CO2;
    };

    Sample SampleRaw() {
        // Gaussian noise around baseline plus occasional spike
        double CO = m_baseCO  + m_norm(m_rng) * m_sigmaCO;
        double NOx = m_baseNOx + m_norm(m_rng) * m_sigmaNOx;
        double HC = m_baseHC  + m_norm(m_rng) * m_sigmaHC;
        double CO2d = (double)m_baseCO2 + m_norm(m_rng) * m_sigmaCO2;

        // occasional spike: 1% chance to add a big jump
        if (m_spikeDist(m_rng) > 0.99) {
            CO *= 2.0;
            NOx *= 2.0;
            HC *= 2.5;
            CO2d += 300;
        }

        if (CO < 0) CO = 0;
        if (NOx < 0) NOx = 0;
        if (HC < 0) HC = 0;
        if (CO2d < 0) CO2d = 0;

        return Sample{CO, NOx, HC, static_cast<uint32_t>(std::round(CO2d))};
    }

private:
    uint32_t m_id;
    FuelType m_fuel;
    double m_baseCO, m_baseNOx, m_baseHC;
    uint32_t m_baseCO2;
    double m_sigmaCO, m_sigmaNOx, m_sigmaHC, m_sigmaCO2;

    std::random_device m_rd;
    std::mt19937 m_rng;
    std::normal_distribution<double> m_norm;
    std::uniform_real_distribution<double> m_spikeDist;
};


/* EmissionApp: custom application that periodically generates emission samples,
   filters them (moving average) and sends to server over UDP. Also logs to CSV. */
class EmissionApp : public Application {
public:
    EmissionApp() : m_socket(0), m_peer(), m_running(false), m_sendEvent(), m_id(0), m_emModel(nullptr) {}
    virtual ~EmissionApp() { m_socket = 0; }

    void Setup(uint32_t id, Ptr<VehicleEmissionModel> model, Ipv4Address serverAddr, uint16_t serverPort,
               double interval, uint32_t filterWindow, std::ofstream* csvOut)
    {
        m_id = id;
        m_emModel = model;
        m_serverAddr = serverAddr;
        m_serverPort = serverPort;
        m_interval = interval;
        m_filterWindow = filterWindow;
        m_csvOut = csvOut;
    }

    virtual void StartApplication() override {
        m_running = true;
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress remote = InetSocketAddress(m_serverAddr, m_serverPort);
        m_peer = remote;
        // Bind to ephemeral port
        m_socket->Bind();
        // schedule first send with small jitter
        Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
        double jitter = uv->GetValue(0.0, 0.5);
        ScheduleSend(Seconds(jitter));
    }

    virtual void StopApplication() override {
        m_running = false;
        if (m_sendEvent.IsRunning()) Simulator::Cancel(m_sendEvent);
        if (m_socket) {
            m_socket->Close();
            m_socket = 0;
        }
    }

private:
    void ScheduleSend(Time t) {
        m_sendEvent = Simulator::Schedule(t, &EmissionApp::SendEmission, this);
    }

    void SendEmission() {
        if (!m_running) return;

        // 1) Sample raw sensor
        auto raw = m_emModel->SampleRaw();

        // 2) Append into moving average buffers
        pushSample(raw);

        // 3) Calculate filtered sample (moving avg)
        auto filtered = computeFiltered();

        // 4) Create JSON-like payload
        std::ostringstream ss;
        ss << "{";
        ss << "\"vehicleId\":\"VEH-" << std::setw(4) << std::setfill('0') << m_id << "\",";
        ss << "\"time\":" << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds() << ",";
        ss << "\"raw\":{\"CO\":" << raw.CO << ",\"NOx\":" << raw.NOx << ",\"HC\":" << raw.HC << ",\"CO2\":" << raw.CO2 << "},";
        ss << "\"filtered\":{\"CO\":" << filtered.CO << ",\"NOx\":" << filtered.NOx << ",\"HC\":" << filtered.HC << ",\"CO2\":" << filtered.CO2 << "}";
        ss << "}";

        std::string payload = ss.str();
        Ptr<Packet> p = Create<Packet>((uint8_t*)payload.data(), payload.size());
        m_socket->SendTo(p, 0, m_peer);

        // 5) Log to CSV (central writer)
        if (m_csvOut && m_csvOut->is_open()) {
            // CSV: vehicleId, simTime, rawCO, rawNOx, rawHC, rawCO2, filtCO, filtNOx, filtHC, filtCO2
            (*m_csvOut) << "VEH-" << std::setw(4) << std::setfill('0') << m_id << ","
                        << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds() << ","
                        << raw.CO << "," << raw.NOx << "," << raw.HC << "," << raw.CO2 << ","
                        << filtered.CO << "," << filtered.NOx << "," << filtered.HC << "," << filtered.CO2 << "\n";
        }

        NS_LOG_INFO("Vehicle " << m_id << " sent emission payload (len=" << payload.size() << ") at t=" << Simulator::Now().GetSeconds());

        // 6) schedule next
        ScheduleSend(Seconds(m_interval));
    }

    void pushSample(const VehicleEmissionModel::Sample &s) {
        m_CO_buf.push_back(s.CO);
        m_NOx_buf.push_back(s.NOx);
        m_HC_buf.push_back(s.HC);
        m_CO2_buf.push_back((double)s.CO2);

        if (m_CO_buf.size() > m_filterWindow) {
            m_CO_buf.erase(m_CO_buf.begin());
            m_NOx_buf.erase(m_NOx_buf.begin());
            m_HC_buf.erase(m_HC_buf.begin());
            m_CO2_buf.erase(m_CO2_buf.begin());
        }
    }

    VehicleEmissionModel::Sample computeFiltered() {
        double sumCO=0, sumNOx=0, sumHC=0, sumCO2=0;
        for (double v : m_CO_buf) sumCO += v;
        for (double v : m_NOx_buf) sumNOx += v;
        for (double v : m_HC_buf) sumHC += v;
        for (double v : m_CO2_buf) sumCO2 += v;
        size_t n = m_CO_buf.size() ? m_CO_buf.size() : 1;
        return VehicleEmissionModel::Sample{ sumCO/n, sumNOx/n, sumHC/n, static_cast<uint32_t>(std::round(sumCO2/n)) };
    }

private:
    Ptr<Socket> m_socket;
    InetSocketAddress m_peer;
    bool m_running;
    EventId m_sendEvent;
    uint32_t m_id;
    Ptr<VehicleEmissionModel> m_emModel;
    double m_interval;
    uint32_t m_filterWindow;

    // buffers for moving average
    std::vector<double> m_CO_buf, m_NOx_buf, m_HC_buf, m_CO2_buf;

    std::ofstream* m_csvOut;
};

/* server packet sink callback to optionally log received payloads */
static void ReceivePacket(Ptr<Socket> socket, std::ofstream* serverCsv) {
    Address from;
    Ptr<Packet> p;
    while ((p = socket->RecvFrom(from))) {
        int size = p->GetSize();
        std::vector<uint8_t> buf(size);
        p->CopyData(buf.data(), size);
        std::string payload(buf.begin(), buf.end());
        // record into server CSV (vehicleId, timeReceived, payloadLength)
        if (serverCsv && serverCsv->is_open()) {
            *serverCsv << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds() << "," << size << "," << "\"" << payload << "\"" << "\n";
        }
        NS_LOG_INFO("Server received packet len=" << size << " at t=" << Simulator::Now().GetSeconds());
    }
}

int main (int argc, char *argv[])
{
    // CLI args
    CommandLine cmd;
    cmd.AddValue("NumVehicles", "Number of vehicle nodes", g_numVehicles);
    cmd.AddValue("SimTime", "Simulation time (s)", g_simTime);
    cmd.AddValue("PktInterval", "Emission packet interval (s)", g_pktInterval);
    cmd.AddValue("FilterWindow", "Moving-average filter window", g_filterWindow);
    cmd.AddValue("OutCsv", "Output CSV filename", g_outputCsv);
    cmd.AddValue("EnablePcap", "Enable PCAP capture on interfaces", g_enablePcap);
    cmd.AddValue("EnableFlowMonitor", "Enable FlowMonitor", g_enableFlowMonitor);
    cmd.Parse(argc, argv);

    // Logging
    LogComponentEnable("VehicleEmissionSim", LOG_LEVEL_INFO);
    // We'll also set ns3 logging for UDP app if desired
    // LogComponentEnable("UdpSocketImpl", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer vehicles;
    vehicles.Create(g_numVehicles);

    NodeContainer server;
    server.Create(1);

    // Wifi adhoc (YANS) — simple vehicular network model
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211p);
    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer vehicleDevices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Server device — install a vehicle-style device on server as well (acts as RSU)
    NetDeviceContainer serverDevice = waveHelper.Install(wifiPhy, wifiMac, server);

    // Mobility: random waypoint inside 1000x1000 m square
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
    x->SetAttribute("Min", DoubleValue(0.0));
    x->SetAttribute("Max", DoubleValue(1000.0));
    y->SetAttribute("Min", DoubleValue(0.0));
    y->SetAttribute("Max", DoubleValue(1000.0));

    // Set a RandomRectanglePositionAllocator
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=20.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator"));
    mobility.Install(vehicles);

    // Put server at fixed location (center)
    Ptr<ListPositionAllocator> serverPos = CreateObject<ListPositionAllocator>();
    serverPos->Add(Vector(500.0, 500.0, 0.0));
    MobilityHelper mobilityServer;
    mobilityServer.SetPositionAllocator(serverPos);
    mobilityServer.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityServer.Install(server);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);
    internet.Install(server);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ifVeh = ipv4.Assign(vehicleDevices);
    Ipv4InterfaceContainer ifServer = ipv4.Assign(serverDevice);

    // Create server UDP socket to receive emissions
    uint16_t serverPort = 4000;
    Ptr<Socket> recvSocket = Socket::CreateSocket(server.Get(0), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), serverPort);
    recvSocket->Bind(local);
    // prepare server CSV writer
    std::ofstream serverCsv("server_received.csv");
    serverCsv << "time,bytes,payload\n";
    // set callback
    recvSocket->SetRecvCallback(MakeBoundCallback(&ReceivePacket, &serverCsv));

    // Create central CSV writer for emission records
    std::ofstream csvOut;
    csvOut.open(g_outputCsv);
    csvOut << "vehicleId,simTime,rawCO,rawNOx,rawHC,rawCO2,filtCO,filtNOx,filtHC,filtCO2\n";

    // Create EmissionApps on vehicles
    std::vector<Ptr<VehicleEmissionModel>> models;
    std::vector<Ptr<Application>> apps;

    // assign fuel types pseudorandomly and instantiate models
    Ptr<UniformRandomVariable> fuelChooser = CreateObject<UniformRandomVariable>();
    for (uint32_t i = 0; i < g_numVehicles; ++i) {
        FuelType ft;
        double v = fuelChooser->GetValue();
        // assign: 50% diesel, 35% petrol, 15% CNG (example)
        if (v < 0.50) ft = DIESEL;
        else if (v < 0.85) ft = PETROL;
        else ft = CNG;
        Ptr<VehicleEmissionModel> mod = Create<VehicleEmissionModel>(i+1, ft);
        models.push_back(mod);
    }

    // install apps
    for (uint32_t i = 0; i < g_numVehicles; ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<EmissionApp> app = CreateObject<EmissionApp>();
        app->Setup(i+1, models[i], ifServer.GetAddress(0), serverPort, g_pktInterval, g_filterWindow, &csvOut);
        node->AddApplication(app);
        app->SetStartTime(Seconds(1.0 + (i * 0.01))); // stagger
        app->SetStopTime(Seconds(g_simTime - 0.1));
        apps.push_back(app);
    }

    // PCAP capture (optional)
    if (g_enablePcap) {
        wifiPhy.EnablePcapAll("vehicle_emission_sim");
    }

    // FlowMonitor
    FlowMonitorHelper flowMonHelper;
    Ptr<FlowMonitor> flowMonitor;
    if (g_enableFlowMonitor) {
        flowMonitor = flowMonHelper.InstallAll();
    }

    // Run
    Simulator::Stop(Seconds(g_simTime));
    Simulator::Run();

    // Flow stats
    if (g_enableFlowMonitor) {
        flowMonitor->SerializeToXmlFile("flowmon-vehicle-emission.xml", true, true);
    }

    // Close CSVs
    csvOut.close();
    serverCsv.close();

    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished. CSV written: " << g_outputCsv << " , server_received.csv");
    return 0;
}
