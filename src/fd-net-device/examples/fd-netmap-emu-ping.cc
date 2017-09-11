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
 * This example shows how to use the netmap emulation capabilities. It pings
 * a real host by means the netmap emulated device on the simulation host.
 * Also, this example is used to functional testing of the netmap emulation features on real device of ns-3.
 * The example is an extension of the fd-emu-ping example.
 *
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
 * The output of this example will 20 s of ping in presence of 2 s of UDP traffic load
 *
 * PING  10.0.1.2 56(84) bytes of data.
 * Received Response with RTT = +9374000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=0 ttl=64 time=9 ms
 * Received Response with RTT = +8154000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=1 ttl=64 time=8 ms
 * Received Response with RTT = +475000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=2 ttl=64 time=0 ms
 * Received Response with RTT = +372000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=3 ttl=64 time=0 ms
 * Received Response with RTT = +5769000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=4 ttl=64 time=5 ms
 * Received Response with RTT = +28510000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=5 ttl=64 time=28 ms
 * Received Response with RTT = +1206000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=6 ttl=64 time=1 ms
 * Received Response with RTT = +1215000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=7 ttl=64 time=1 ms
 * Received Response with RTT = +1268000.0ns
 * 64 bytes from 10.0.1.2: icmp_seq=8 ttl=64 time=1 ms
 * ...
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
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PingEmulationExample");

static void
StatsSampling (Ptr<QueueDisc> qdisc, Ptr<NetDevice> device, double samplingPeriod)
{
  Simulator::Schedule (Seconds (samplingPeriod), &StatsSampling, qdisc, device, samplingPeriod);
  Ptr<NetmapNetDevice> d = DynamicCast<NetmapNetDevice> (device);

  std::cout << qdisc->GetNPackets () << " packets in the traffic-control queue disc" << std::endl;
  if (d)
    {
      std::cout << d->GetBytesInNetmapTxRing () << " bytes in the netmap tx ring" << std::endl;
    }
}

static void
PingRtt (std::string context, Time rtt)
{
  NS_LOG_UNCOND ("Received Response with RTT = " << rtt);
}

int
main (int argc, char *argv[])
{
  NS_LOG_INFO ("Ping Emulation Example");

  std::string deviceName ("eno1");
  // ping a real host connected back-to-back through the ethernet interfaces
  std::string remote ("10.0.1.2");

  double samplingPeriod = 0.5; // s
  uint32_t packetsSize = 1400; // bytes
  bool bql = false;

  CommandLine cmd;
  cmd.AddValue ("deviceName", "Device name", deviceName);
  cmd.AddValue ("remote", "Remote IP address (dotted decimal only please)", remote);
  cmd.AddValue ("bql", "Enable byte queue limits", bql);
  cmd.Parse (argc, argv);

  Ipv4Address remoteIp (remote.c_str ());
  // the OS IP for the eth0 interfaces is 10.0.1.1, and we set the ns-3 IP for eth0 to 10.0.1.11
  Ipv4Address localIp ("10.0.1.11");
  NS_ABORT_MSG_IF (localIp == "1.2.3.4", "You must change the local IP address before running this example");

  Ipv4Mask localMask ("255.255.255.0");

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
  //  http://www.iana.org/assignments/ethernet-numbers
  //
  // reports that this OUI is unassigned, and so should not conflict with
  // real hardware on your net.  It may raise all kinds of red flags in a
  // real environment to have packets from a device with an obviously bogus
  // OUI flying around.  Be aware.
  //
  NS_LOG_INFO ("Create Device");
  EmuFdNetDeviceHelper emu;

  // set the netmap emulation mode
  emu.SetNetmapMode ();

  emu.SetDeviceName (deviceName);
  NetDeviceContainer devices = emu.Install (node);
  Ptr<NetDevice> device = devices.Get (0);
  device->SetAttribute ("Address", Mac48AddressValue (Mac48Address::Allocate ()));

  //Ptr<Queue> queue = CreateObject<DropTailQueue> ();
  //device->SetQueue (queue);
  //node->AddDevice (device);

  //
  // Add a default internet stack to the node.  This gets us the ns-3 versions
  // of ARP, IPv4, ICMP, UDP and TCP.
  //
  NS_LOG_INFO ("Add Internet Stack");
  InternetStackHelper internetStackHelper;
  internetStackHelper.Install (node);

  // we install the pfifo_fast queue disc on the netmap emulated device and we sample the
  // queue disc backlog in packets and the inflight in the netmap tx ring in bytes
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  if (bql)
    {
      tch.SetQueueLimits ("ns3::DynamicQueueLimits");
    }

  QueueDiscContainer qdiscs = tch.Install (devices);
  Simulator::Schedule (Seconds (samplingPeriod), &StatsSampling, qdiscs.Get (0), device, samplingPeriod);

  NS_LOG_INFO ("Create IPv4 Interface");
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  uint32_t interface = ipv4->AddInterface (device);
  Ipv4InterfaceAddress address = Ipv4InterfaceAddress (localIp, localMask);
  ipv4->AddAddress (interface, address);
  ipv4->SetMetric (interface, 1);
  ipv4->SetUp (interface);

  //
  // When the ping application sends its ICMP packet, it will happily send it
  // down the ns-3 protocol stack.  We set the IP address of the destination
  // to the address corresponding to example.com above.  This address is off
  // our local network so we have got to provide some kind of default route
  // to ns-3 to be able to get that ICMP packet forwarded off of our network.
  //
  // You have got to provide an IP address of a real host that you can send
  // real packets to and have them forwarded off of your local network.  One
  // thing you could do is a 'netstat -rn' command and find the IP address of
  // the default gateway on your host and add it below, replacing the
  // "1.2.3.4" string.
  //
  Ipv4Address gateway ("10.0.1.2");
  NS_ABORT_MSG_IF (gateway == "1.2.3.4", "You must change the gateway IP address before running this example");

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);
  staticRouting->SetDefaultRoute (gateway, interface);

  //
  // Create the ping application.  This application knows how to send
  // ICMP echo requests.  Setting up the packet sink manually is a bit
  // of a hassle and since there is no law that says we cannot mix the
  // helper API with the low level API, let's just use the helper.
  //
  NS_LOG_INFO ("Create V4Ping Appliation");
  Ptr<V4Ping> app = CreateObject<V4Ping> ();
  app->SetAttribute ("Remote", Ipv4AddressValue (remoteIp));
  app->SetAttribute ("Verbose", BooleanValue (true) );
  app->SetAttribute ("Interval", TimeValue (Seconds (samplingPeriod)));
  node->AddApplication (app);
  app->SetStartTime (Seconds (1.0));
  app->SetStopTime (Seconds (21.0));

  //
  // Give the application a name.  This makes life much easier when constructing
  // config paths.
  //
  Names::Add ("app", app);

  //
  // Hook a trace to print something when the response comes back.
  //
  Config::Connect ("/Names/app/Rtt", MakeCallback (&PingRtt));

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packetsSize));

  // UDP traffic load
  OnOffHelper onoff ("ns3::UdpSocketFactory", Ipv4Address::GetAny ());
  onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetsSize));
  onoff.SetAttribute ("DataRate", StringValue ("100Mbps"));
  ApplicationContainer apps;

  InetSocketAddress rmt (remoteIp, 7000);
//   rmt.SetTos (0xb8);
  AddressValue remoteAddress (rmt);
  onoff.SetAttribute ("Remote", remoteAddress);

  apps.Add (onoff.Install (node));
  apps.Start (Seconds (7.0));
  apps.Stop (Seconds (12.0));

  //
  // Enable a promiscuous pcap trace to see what is coming and going on our device.
  //
  emu.EnablePcap ("emu-ping", device, true);

  //
  // Now, do the actual emulation.
  //
  NS_LOG_INFO ("Run Emulation.");
  Simulator::Stop (Seconds (22.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
