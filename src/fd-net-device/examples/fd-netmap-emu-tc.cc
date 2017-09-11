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
 * Author: Pasquale Imputato <p.imputato@gmail.com>
 *
 */

/*
 *                                       node
 *                          --------------------------------
 *                          |                              |
 *                          |   pfifo_fast    pfifo_fast   |
 *                          |   interface 0   interface 1  |
 *                          |        |             |       |
 *                          --------------------------------
 *                                   |             |
 *   1 Gbps access incoming link     |             |           100 Mbps bottleneck outgoing link
 * -----------------------------------             -----------------------------------
 *
 * This example build a node with two interface in netmap mode.
 * The user can explore different qdiscs behaviours on the backlog of a device emulated with netmap.
 *
 * Requirements
 * ************
 * This script can be used if the host machine provides a netmap installation
 * and the ns-3 configuration was made with the --enable-sudo option.
 *
 * Before to use this script, the user must load the netmap kernel module and set the
 * interface in promisc mode.
 *
 * Finally, the emulation in netmap mode requires that the ns-3 IP for the emulated interface
 * must be different from the OS IP for that interface but on the same subnet. Conversely, the packets
 * destinated to the OS IP for that interface will be placed in the sw rings of the netmap.
 *
 */

#include "ns3/abort.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TrafficControlEmu");

void
TcPacketsInQueue (Ptr<QueueDisc> q, Ptr<NetmapNetDevice> d, Ptr<OutputStreamWrapper> stream)
{
  Simulator::Schedule (Seconds (0.2), &TcPacketsInQueue, q, d, stream);
   *stream->GetStream () << Simulator::Now ().GetSeconds () << " backlog " << q->GetNPackets () << "p " << q->GetNBytes () << "b "<< " dropped " 
  << q->GetTotalDroppedPackets () << "p " << q->GetTotalDroppedBytes () << "b " << std::endl;
}

void
Inflight (Ptr<NetmapNetDevice> d, Ptr<OutputStreamWrapper> stream)
{
  Simulator::Schedule (Seconds (0.2), &Inflight, d, stream);
   *stream->GetStream () << d->GetBytesInNetmapTxRing () << std::endl;
}

int
main (int argc, char *argv[])
{
  NS_LOG_INFO ("Ping Emulation Example");

  std::string deviceName0 ("enx503f56005a2a");
  std::string deviceName1 ("eno1");
  std::string ip0 ("10.0.1.2");
  std::string ip1 ("10.0.2.1");
  std::string mask0 ("255.255.255.0");
  std::string mask1 ("255.255.255.0");
  std::string m0 ("00:00:00:aa:00:01");
  std::string m1 ("00:00:00:aa:00:02");

  bool writer = false;

  //
  // Allow the user to override any of the defaults at run-time, via
  // command-line arguments
  //
  CommandLine cmd;
  cmd.AddValue ("deviceName0", "Device name", deviceName0);
  cmd.AddValue ("deviceName1", "Device name", deviceName1);
  cmd.AddValue ("ip0", "Local IP address (dotted decimal only please)", ip0);
  cmd.AddValue ("ip1", "Local IP address (dotted decimal only please)", ip1);
  cmd.AddValue ("mask0", "Local IP address (dotted decimal only please)", mask0);
  cmd.AddValue ("mask1", "Local IP address (dotted decimal only please)", mask1);
  cmd.AddValue ("m0", "Local IP address (dotted decimal only please)", m0);
  cmd.AddValue ("m1", "Local IP address (dotted decimal only please)", m1);
  cmd.AddValue ("writer", "Enable write stats", writer);
  cmd.Parse (argc, argv);

  Ipv4Address localIp0 (ip0.c_str ());
  Ipv4Address localIp1 (ip1.c_str ());
  Ipv4Mask netmask0 (mask0.c_str ());
  Ipv4Mask netmask1 (mask1.c_str ());
  Mac48AddressValue mac0 (m0.c_str ());
  Mac48AddressValue mac1 (m1.c_str ());

  //
  // Since we are using a real piece of hardware we need to use the realtime
  // simulator.
  //
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));

  //
  // Since we are going to be talking to real-world machines, we need to enable
  // calculation of checksums in our protocols.
  //
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  //
  // In such a simple topology, the use of the helper API can be a hindrance
  // so we drop down into the low level API and do it manually.
  //
  // First we need a single node.
  //
  NS_LOG_INFO ("Create Node");
  Ptr<Node> node = CreateObject<Node> ();

  //
  // Create an emu device, allocate a MAC address and point the device to the
  // Linux device name.  The device needs a transmit queueing discipline so
  // create a droptail queue and give it to the device.  Finally, "install"
  // the device into the node.
  //
  // Do understand that the ns-3 allocated MAC address will be sent out over
  // your network since the emu net device will spoof it.  By default, this
  // address will have an Organizationally Unique Identifier (OUI) of zero.
  // The Internet Assigned Number Authority IANA
  //
  //  http://www.iana.org/assignments/ethernet-numberslocalIp
  //
  // reports that this OUI is unassigned, and so should not conflict with
  // real hardware on your net.  It may raise all kinds of red flags in a
  // real environment to have packets from a device with an obviously bogus
  // OUI flying around.  Be aware.
  //
  NS_LOG_INFO ("Create Device 0");
  EmuFdNetDeviceHelper emu0;

  emu0.SetNetmapMode ();

  emu0.SetDeviceName (deviceName0);
  NetDeviceContainer devices0 = emu0.Install (node);
  Ptr<NetDevice> device0 = devices0.Get (0);
  device0->SetAttribute ("Address", mac0);

  NS_LOG_INFO ("Create Device 1");
  EmuFdNetDeviceHelper emu1;

  emu1.SetNetmapMode ();

  emu1.SetDeviceName (deviceName1);
  NetDeviceContainer devices1 = emu1.Install (node);
  Ptr<NetDevice> device1 = devices1.Get (0);
  device1->SetAttribute ("Address", mac1);

  //
  // Add a default internet stack to the node.  This gets us the ns-3 versions
  // of ARP, IPv4, ICMP, UDP and TCP.
  //
  NS_LOG_INFO ("Add Internet Stack");
  InternetStackHelper internetStackHelper;
  internetStackHelper.Install (node);

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();

  NS_LOG_INFO ("Create IPv4 Interface 0");
  uint32_t interface0 = ipv4->AddInterface (device0);
  Ipv4InterfaceAddress address0 = Ipv4InterfaceAddress (ip0.c_str (), netmask0);
  ipv4->AddAddress (interface0, address0);
  ipv4->SetMetric (interface0, 0);
  ipv4->SetForwarding (interface0, true);
  ipv4->SetUp (interface0);

  NS_LOG_INFO ("Create IPv4 Interface 1");
  uint32_t interface1 = ipv4->AddInterface (device1);
  Ipv4InterfaceAddress address1 = Ipv4InterfaceAddress (ip1.c_str (), netmask1);
  ipv4->AddAddress (interface1, address1);
  ipv4->SetMetric (interface1, 0);
  ipv4->SetForwarding (interface1, true);
  ipv4->SetUp (interface1);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  tch.Install (devices0);

  TrafficControlHelper tch2;
  tch2.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  
  QueueDiscContainer qdiscs = tch2.Install (devices1);

  Ptr<QueueDisc> q = qdiscs.Get (0);
  Ptr<NetmapNetDevice> d = StaticCast <NetmapNetDevice> (device1);

  if (writer)
    {
      AsciiTraceHelper ascii;
      Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("ns-3-tc-stats.txt");
      Simulator::Schedule (Seconds (0.5), &TcPacketsInQueue, q, d, stream);
      
      Ptr<OutputStreamWrapper> stream2 = ascii.CreateFileStream ("ns-3-inflight.txt");
      Simulator::Schedule (Seconds (0.5), &Inflight, d, stream2);

      Ipv4GlobalRoutingHelper g;
      Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("ns-3-routes", std::ios::out);
      g.PrintRoutingTableAllAt (Seconds (3), routingStream);
    }

  NS_LOG_INFO ("Run Emulation.");
  Simulator::Stop (Seconds (70.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
