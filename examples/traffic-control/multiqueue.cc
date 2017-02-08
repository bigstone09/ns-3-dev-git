/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Universita' degli Studi di Napoli Federico II
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Pasquale Imputato <p.imputato@gmail.com>
 *          Stefano Avallone <stefano.avallone@unina.it>
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

// Network Topology
//
//   WiFi 10.1.2.0
//                  AP
// *                *
// |                |       10.1.1.0
// n2               n0 ------------------- n1
//                       point-to-point

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Multiqueue");

void
TcPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

int
main (int argc, char *argv[])
{
  double simulationTime = 60; //seconds
  bool tracing = false;
  std::string queueDiscType = "PfifoFast";
  uint32_t queueSize = 100;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable tc tracing", tracing);
  cmd.AddValue ("queueDiscType", "AP queue disc type in {PfifoFast, MqPfifoFast, MqFqCoDel}", queueDiscType);
  cmd.AddValue ("queueSize", "Devices queue size in packets", queueSize);
  cmd.Parse (argc,argv);

  Config::SetDefault ("ns3::QueueBase::MaxPackets", UintegerValue (queueSize));

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (1);
  NodeContainer wifiApNode = p2pNodes.Get (0);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "QosSupported", BooleanValue (true));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

//   mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
//                              "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.InstallAll ();

  // Traffic control configurations
  // 1) Traffic control configuration for the AP node
  TrafficControlHelper tch;
  if (queueDiscType.compare ("PfifoFast") == 0)
    {
      tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
    }
  else if (queueDiscType.compare ("MqPfifoFast") == 0)
    {
      uint32_t handle = tch.SetRootQueueDisc ("ns3::MqQueueDisc");
      TrafficControlHelper::ClassIdList cls = tch.AddQueueDiscClasses (handle, 4, "ns3::QueueDiscClass");
      tch.AddChildQueueDiscs (handle, cls, "ns3::PfifoFastQueueDisc");
    }
  else if (queueDiscType.compare ("MqFqCoDel") == 0)
    {
      uint32_t handle = tch.SetRootQueueDisc ("ns3::MqQueueDisc");
      TrafficControlHelper::ClassIdList cls = tch.AddQueueDiscClasses (handle, 4, "ns3::QueueDiscClass");
      TrafficControlHelper::HandleList hdl = tch.AddChildQueueDiscs (handle, cls, "ns3::FqCoDelQueueDisc");
      for (auto h : hdl)
        {
          tch.AddPacketFilter (h, "ns3::FqCoDelIpv4PacketFilter");
        }
    }
  else
    {
      NS_ABORT_MSG ("--queueDiscType not valid");
    }

  QueueDiscContainer qdisc = tch.Install (apDevices);
  // 2) The p2p nodes and the WiFi node have a default traffic control configuration for single queue devices
  // (of type pfifo-fast)

  if (tracing == true)
    {
      Ptr<QueueDisc> q = qdisc.Get (3);
      q->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TcPacketsInQueueTrace));
    }

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces, staInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  staInterfaces = address.Assign (staDevices);
  address.Assign (apDevices);

  uint32_t payloadSize = 1400;
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  // flows
  ApplicationContainer sinkApps, sourceApps;
  uint16_t port1 = 7;

  Address localAddress1 (InetSocketAddress (Ipv4Address::GetAny (), port1));
  PacketSinkHelper packetSinkHelper1 ("ns3::TcpSocketFactory", localAddress1);

  sinkApps.Add (packetSinkHelper1.Install (wifiStaNodes.Get (0)));

  OnOffHelper onoff1 ("ns3::TcpSocketFactory", Ipv4Address::GetAny ());
  onoff1.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff1.SetAttribute ("DataRate", StringValue ("20Mbps"));

  InetSocketAddress dest1 (staInterfaces.GetAddress (0), port1);
  dest1.SetTos (0x00);

  AddressValue remoteAddress1 (dest1);
  onoff1.SetAttribute ("Remote", remoteAddress1);

  sourceApps.Add (onoff1.Install (p2pNodes.Get (1)));


  uint16_t port2 = 8;

  Address localAddress2 (InetSocketAddress (Ipv4Address::GetAny (), port2));
  PacketSinkHelper packetSinkHelper2 ("ns3::TcpSocketFactory", localAddress2);

  sinkApps.Add (packetSinkHelper2.Install (wifiStaNodes.Get (0)));

  OnOffHelper onoff2 ("ns3::TcpSocketFactory", Ipv4Address::GetAny ());
  onoff2.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff2.SetAttribute ("DataRate", StringValue ("20Mbps"));

  InetSocketAddress dest2 (staInterfaces.GetAddress (0), port2);
  dest2.SetTos (0x98);

  AddressValue remoteAddress2 (dest2);
  onoff2.SetAttribute ("Remote", remoteAddress2);

  sourceApps.Add (onoff2.Install (p2pNodes.Get (1)));

  // start and stop apps
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime + 0.1));

  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (simulationTime + 0.1));

//   FlowMonitorHelper flowmon;
//   Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (simulationTime + 5));

  WiMeshFlowMon* flowMon = new WiMeshFlowMon ();
  flowMon->Enable (1, simulationTime, true, "mqWriter");
 
  Simulator::Run ();

  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  for (uint32_t i = 0; i < 2; i++)
    {
      std::cout << std::endl << "*** Application " << i << " ***" << std::endl;
      double thr = 0;
      uint32_t totalPacketsThr = DynamicCast<PacketSink> (sinkApps.Get (i))->GetTotalRx ();
      thr = totalPacketsThr * 8 / (simulationTime * 1000000.0); //Mbit/s
      std::cout << "  Rx Bytes: " << totalPacketsThr << std::endl;
      std::cout << "  Average Goodput: " << thr << " Mbit/s" << std::endl;
    }

  flowMon->GetFlowMonitor ()->SerializeToXmlFile("mqFlowMonitor.xml", true, true);
// 
//   Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
//   std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
//   std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
//   for (int i = 1; i <= 2; i++)
//     {
//       std::cout << "Flow " << i << " statistics" << std::endl;
//       std::cout << "  Tx Packets:   " << stats[i].txPackets << std::endl;
//       std::cout << "  Tx Bytes:   " << stats[i].txBytes << std::endl;
//       std::cout << "  Offered Load: " << stats[i].txBytes * 8.0 / (stats[1].timeLastTxPacket.GetSeconds () - stats[1].timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
//       std::cout << "  Rx Packets:   " << stats[i].rxPackets << std::endl;
//       std::cout << "  Rx Bytes:   " << stats[i].rxBytes << std::endl;
// 
//       std::cout << "  Delay sum:   " << stats[i].delaySum / 1000000 << " ms "<< std::endl;
//       std::cout << "  Lost packets:   " << stats[i].lostPackets << std::endl;
//     }
// 
// //   monitor->SerializeToXmlFile("mqFlowMonitor.xml", true, true);


  Simulator::Destroy ();
  return 0;
}
