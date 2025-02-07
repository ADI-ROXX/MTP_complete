#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ieee80211pSimulation");


// time of entry to mac
static std::map<uint32_t, Time> g_macTxTimeMap;
static std::map<uint32_t, Time> g_phyTxBeginTimeMap;
static std::vector<Time> g_macAccessDelays;

static void MacTxCallback(Ptr<const Packet> packet)
{
  g_macTxTimeMap[packet->GetUid()] = Simulator::Now();
}

static void PhyTxBeginCallback(Ptr<const Packet> packet, double txPowerW)
{
    g_phyTxBeginTimeMap[packet->GetUid()] = Simulator::Now();
}

static void PhyTxEndCallback(Ptr<const Packet> packet)
{
    Time start = g_macTxTimeMap[packet->GetUid()];
    Time end = g_phyTxBeginTimeMap[packet->GetUid()];

    Time delay = end - start;
    g_macAccessDelays.push_back(delay);
    NS_LOG_UNCOND("Packet " << packet->GetUid() << " took " << delay.GetMicroSeconds() << "us");
}


void ReceivePacket(Ptr<const Packet> packet, const Address& address)
{
  std::cout << "At time "
            << Simulator::Now().GetSeconds()
            << " s, received packet of size "
            << packet->GetSize()
            << " bytes from "
            << InetSocketAddress::ConvertFrom(address).GetIpv4()
            << std::endl;
}

/* Function to broadcast a packet and schedule the next broadcast */
void
BroadcastPacket(Ptr<Socket> socket, Ptr<ExponentialRandomVariable> rand, uint32_t packetSize)
{
  Ptr<Packet> packet = Create<Packet>(packetSize);
  socket->Send(packet);

  uint32_t nodeId = socket->GetNode()->GetId();
  std::cout << "Node " << nodeId
            << " sent a packet at "
            << Simulator::Now().GetSeconds()
            << " s"
            << std::endl;

  // Schedule the next broadcast after a random interval
  double nextInterval = rand->GetValue();
  Simulator::Schedule(Seconds(nextInterval), &BroadcastPacket, socket, rand, packetSize);
}

int
main(int argc, char* argv[])
{
  // Parameters
  uint32_t packetSize      = 1000;
  double meanArrivalTime   = 30;
  uint8_t n_vehicles       = 10;
  double headway           = 12.0;
  double simulationTime    = 20.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size in bytes of each packet", packetSize);
  cmd.AddValue("meanArrivalTime", "Mean arrival time (seconds) for exponential distribution",
               meanArrivalTime);
  cmd.AddValue("nVehicles", "Number of vehicles", nVehicles);
  cmd.AddValue("headway", "Distance (meters) between consecutive vehicles", headway);
  cmd.AddValue("simulationTime", "Total duration of the simulation (seconds)", simulationTime);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::ConstantRateWifiManager::DataMode",
                     StringValue("OfdmRate18Mbps"));
  Config::SetDefault("ns3::ConstantRateWifiManager::ControlMode",
                     StringValue("OfdmRate18Mbps"));

  NodeContainer nodes;
  nodes.Create(n_vehicles);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> channel = wifiChannel.Create();

  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(channel);
  wifiPhy.Set("ChannelWidth", UintegerValue(20));

  // 802.11p in ad-hoc (OCB) mode
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  QosWaveMacHelper wifi80211pMac = QosWaveMacHelper::Default();

  // Set remote station manager for 802.11p
  wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode",    StringValue("OfdmRate27Mbps"),
                                     "ControlMode", StringValue("OfdmRate27Mbps"));

  NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifi80211pMac, nodes);

  // Position each node in a linear topology
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < n_vehicles; ++i)
    {
      positionAlloc->Add(Vector(i * headway, 0.0, 0.0));
    }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Install IP stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Create a UDP sink on each node listening on port 8080
  uint16_t port = 8080;
  for (uint32_t i = 0; i < nodes.GetN(); i++)
    {
      Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
      PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
      ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(i));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(simulationTime));

      // Hook our ReceivePacket() callback to the PacketSink "Rx" trace
      Ptr<Application> app = sinkApp.Get(0);
      app->TraceConnectWithoutContext("Rx", MakeCallback(&ReceivePacket));
    }

  // Create a sending socket on each node, configured for broadcast
  std::vector<Ptr<Socket>> sendSockets;
  for (uint32_t i = 0; i < nodes.GetN(); i++)
    {
      Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
      socket->SetAllowBroadcast(true);

      InetSocketAddress broadcastAddr = InetSocketAddress(Ipv4Address("10.1.1.255"), port);
      socket->Connect(broadcastAddr);
      sendSockets.push_back(socket);
    }

  // Exponential distribution for inter-broadcast times
  Ptr<ExponentialRandomVariable> rand = CreateObject<ExponentialRandomVariable>();
  rand->SetAttribute("Mean", DoubleValue(meanArrivalTime));

  // Schedule each node's first packet
  for (uint32_t i = 0; i < sendSockets.size(); i++)
    {
      double startDelay = rand->GetValue(0.0, 1.0);
      Simulator::Schedule(Seconds(startDelay),
                          &BroadcastPacket,
                          sendSockets[i],
                          rand,
                          packetSize);
    }

  // ---------------------------------------------------------------------
  // 4) Connect the new MacTx / PhyTxBegin trace sources for each Wifi device
  // ---------------------------------------------------------------------
  for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
      // Cast the NetDevice to a WifiNetDevice
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(devices.Get(i));
      if (!wifiDev)
        {
          continue; // Should not happen with wifi80211p, but just in case
        }

      wifiDev->GetMac()->TraceConnectWithoutContext("MacTx", MakeCallback(&MacTxCallback));
      Ptr<WifiPhy> phy = wifiDev->GetPhy();
      phy->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&PhyTxBeginCallback));
      phy->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&PhyTxEndCallback));

    }

  // ---------------------------------------------------------------------
  // Run simulation
  // ---------------------------------------------------------------------
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // ---------------------------------------------------------------------
  // 5) Compute mean MAC access delay at the end of simulation
  // ---------------------------------------------------------------------
  double sumDelays = 0.0;
  for (Time d : g_macAccessDelays)
    {
      sumDelays += d.GetDouble(); // by default in seconds
    }
  double meanDelaySec = 0.0;
  if (!g_macAccessDelays.empty())
    {
      meanDelaySec = sumDelays / g_macAccessDelays.size();
    }

  std::cout << "\n=== MAC Access Delay Statistics ===\n";
  std::cout << "Number of transmitted packets: " << g_macAccessDelays.size() << std::endl;
  std::cout << "Mean MAC access delay: " << meanDelaySec << " s" << std::endl;

  Simulator::Destroy();
  return 0;
}
