/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This example demonstrates sending a UDP packet from one node to another using the 802.11n standard with channel bonding (40MHz).
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211nChannelBondingExample");

int 
main (int argc, char *argv[])
{
  // Enable logging for debugging (optional)
  // Uncomment the following lines to enable detailed logging
  // LogComponentEnable ("Wifi80211nChannelBondingExample", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Install Mobility Model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Configure Wi-Fi PHY and Channel
  YansWifiPhyHelper phy = YansWifiPhyHelper();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Configure Wi-Fi with 802.11n standard
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n); // 5GHz band for 802.11n

  // Enable channel bonding by setting the channel width to 40MHz
  phy.Set ("ChannelWidth", UintegerValue (40));

  // Configure the Remote Station Manager to use a specific MCS (Modulation and Coding Scheme)
  // HtMcs7 corresponds to the highest MCS for 40MHz channel width, providing maximum throughput
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("HtMcs7"),
                                "ControlMode", StringValue ("HtMcs0"));

  // Configure MAC layer
  WifiMacHelper mac;
  Ssid ssid = Ssid ("wifi-80211n-ssid");

  // Set up the MAC for the STA (Station)
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  // Install Wi-Fi devices on nodes
  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create a UDP Echo Server on node 1
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Create a UDP Echo Client on node 0
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable PCAP Tracing (optional)
  phy.EnablePcap ("wifi-80211n-channel-bonding", devices.Get (0));

  // Run the simulation
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
