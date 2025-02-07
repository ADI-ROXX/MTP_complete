#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HomogeneousPlatooning");

int main(int argc, char *argv[]) {
    // Simulation parameters with default values
    uint32_t nVehicles = 5;         // Number of vehicles in the platoon
    double spacing = 10.0;          // Distance between vehicles (meters)
    double speed = 15.0;            // Constant speed (m/s)
    double simulationTime = 20.0;   // Simulation duration (seconds)
    std::string animFile = "platooning-animation.xml"; // NetAnim output file

    // Parse command-line arguments
    CommandLine cmd;
    cmd.AddValue("nVehicles", "Number of vehicles in the platoon", nVehicles);
    cmd.AddValue("spacing", "Spacing between vehicles (meters)", spacing);
    cmd.AddValue("speed", "Constant speed of the vehicles (m/s)", speed);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("animFile", "Name of the NetAnim output file", animFile);
    cmd.Parse(argc, argv);

    // Create vehicle nodes
    NodeContainer vehicles;
    vehicles.Create(nVehicles);

    // Install Internet stack (optional, useful for communication applications)
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Configure mobility: Single lane along the x-axis with constant speed
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Initialize positions and velocities
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        Ptr<ConstantVelocityMobilityModel> mob =
            vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetPosition(Vector(i * spacing, 0.0, 0.0)); // Single lane (y=0)
        mob->SetVelocity(Vector(speed, 0.0, 0.0));       // Constant speed along x-axis
    }

    // Configure WiFi communication (optional for platoon control)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWaveMacHelper wifiMac = QosWaveMacHelper();
    WaveHelper waveHelper;
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Assign IP addresses (optional)
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Enable pcap tracing (optional)
    // wifiPhy.EnablePcapAll("platooning");

    // Configure NetAnim
    AnimationInterface anim(animFile);

    // Optional: Set node positions in NetAnim
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        anim.SetConstantPosition(vehicles.Get(i), i * spacing, 0.0);
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation completed. Animation file: " << animFile);
    return 0;
}
