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
 *
 * This example is aimed at measuring the maximum tx rate in pps achievable with
 * the NetmapNetDevice and compare it with the FdNetDevice one.
 *
 * The script provides two levels of measure, at Write level (lower level) or at SendFrom level.
 *
 * The user can run this script with the desired emulation mode (standard mode or netmap mode) and the desired test level
 * (write level or send level).
 *
 * The emulated device must be connected in a back-to-back scenario and in promiscuous mode.
 *
 * The output of this example will be the number of packets writed per second,
 * the actual period of measure in ms, the number of write failed per second, and the estimated throughput in Mbps.
 *
 * Example: this script in netmap mode and test level 0
 * (./waf --run "src/fd-net-device/examples/fd-netmap-emu-send --netmapMode=true --mode=0")
 * on an Intel i7 host with an Intel e1000e ethernet device, netmap in emulated mode (i.e., with the e1000e unchenged driver)
 * connected back-to-back to an usb ethernet adapter on another host provides:
 *
 * Writing
 * 1380260 packets sent in 1000 ms, failed 0 (706 Mbps estimanted throughput)
 * 1385719 packets sent in 1000 ms, failed 0 (709 Mbps estimanted throughput)
 * 1385504 packets sent in 1000 ms, failed 0 (709 Mbps estimanted throughput)
 * 1385398 packets sent in 1000 ms, failed 0 (709 Mbps estimanted throughput)
 * 1384558 packets sent in 1000 ms, failed 0 (708 Mbps estimanted throughput)
 * 1330048 packets sent in 1000 ms, failed 0 (680 Mbps estimanted throughput)
 * 1352369 packets sent in 1000 ms, failed 0 (692 Mbps estimanted throughput)
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
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

#include <chrono>
#include <unistd.h> // usleep

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NetmapEmulationSendExample");

// this function sends a number of times a packet by means the SendFrom method or
// the Write method (depending from the mode value) of a FdNetDevice or of a NetmapNetDevice
// (depending from the emulation mode).

static void
Send (Ptr<FdNetDevice> device, int mode)
{

  int packets = 10000000;

  Mac48Address sender = Mac48Address("00:00:00:00:00:01");
  Mac48Address receiver = Mac48Address("ff:ff:ff:ff:ff:ff");

  int packetsSize = 64;
  Ptr<Packet> packet = Create<Packet> (packetsSize);
  EthernetHeader header;
 
  ssize_t len =  (size_t) packet->GetSize ();
  uint8_t *buffer = (uint8_t*)malloc (len);
  packet->CopyData (buffer, len);

  int sent = 0;
  int failed = 0;

  Ptr<NetmapNetDevice> dev = DynamicCast<NetmapNetDevice> (device);

  std::cout << ((mode == 0) ? "Writing" : "Sending") << std::endl;

  // period to print the stats
  std::chrono::milliseconds period (1000); // 1s

  auto t1 = std::chrono::high_resolution_clock::now();

  while (packets > 0)
    {

      int batch = 1;

      // in case of netmap emulated device we check for
      // available slot in the netmap transmission ring
      if (dev)
        {
          // we can adapt the number of packets that
          // we send to device in case of netmap emulated device with
          // batch = dev->GetSpaceInNetmapTxRing ();

          while (dev->GetTxQueue ()->IsStopped ())
            {
              // we are waiting for available slots in the
              // netmap ring
              usleep (10);
            }
        }

      // actual send a number of back copies of the packet
      for (int i = 0; i < batch; i++)
        {
          if (mode == 1)
            {
              if (device->SendFrom (packet, sender, receiver, 0) == false)
                {
                  failed++;
                }
              sent++;
              packet->RemoveHeader (header);
            }

          if (mode == 0)
            {
              if (device->Write (buffer, len) != len)
                {
                  failed ++;
                }
              sent++;
            }
        }

      auto t2 = std::chrono::high_resolution_clock::now();

      if (t2 - t1 >= period)
        {

          // print stats
          std::chrono::duration<double, std::milli> dur = (t2 - t1); // in ms
          double estimatedThr = ((sent - failed) * packetsSize * 8) / 1000000; // in Mbps

          std::cout << sent << " packets sent in "<< dur.count () << " ms, failed " << failed <<" (" << estimatedThr << " Mbps estimanted throughput)" << std::endl ;

          sent = 0;
          failed = 0;
          t1 = std::chrono::high_resolution_clock::now();

        }

      packets = packets - batch;

    }

}

int
main (int argc, char *argv[])
{
  NS_LOG_INFO ("Netmap Send Example");

  std::string deviceName ("eno1");

  int mode = 0;
  bool netmapMode = false;

  CommandLine cmd;
  cmd.AddValue ("deviceName", "Device name", deviceName);
  cmd.AddValue ("mode", "Enable send (1) or write (0) level test", mode);
  cmd.AddValue ("netmapMode", "Enable the netmap emulation mode", netmapMode);
  cmd.Parse (argc, argv);

  // the OS IP for the eth0 interfaces is 10.0.1.1, and we set the ns-3 IP for eth0 to 10.0.1.11
  Ipv4Address localIp ("10.0.1.11");
  NS_ABORT_MSG_IF (localIp == "1.2.3.4", "You must change the local IP address before running this example");

  Ipv4Mask localMask ("255.255.255.0");


  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));


  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));


  NS_LOG_INFO ("Create Node");
  Ptr<Node> node = CreateObject<Node> ();


  NS_LOG_INFO ("Create Device");
  EmuFdNetDeviceHelper emu;

  // set the netmap emulation mode
  if (netmapMode)
    {
      emu.SetNetmapMode ();
    }

  emu.SetDeviceName (deviceName);
  NetDeviceContainer devices = emu.Install (node);
  Ptr<NetDevice> device = devices.Get (0);
  device->SetAttribute ("Address", Mac48AddressValue (Mac48Address::Allocate ()));


  NS_LOG_INFO ("Add Internet Stack");
  InternetStackHelper internetStackHelper;
  internetStackHelper.Install (node);


  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  tch.Install (devices);


  NS_LOG_INFO ("Create IPv4 Interface");
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  uint32_t interface = ipv4->AddInterface (device);
  Ipv4InterfaceAddress address = Ipv4InterfaceAddress (localIp, localMask);
  ipv4->AddAddress (interface, address);
  ipv4->SetMetric (interface, 1);
  ipv4->SetUp (interface);


  Ipv4Address gateway ("10.0.1.2");
  NS_ABORT_MSG_IF (gateway == "1.2.3.4", "You must change the gateway IP address before running this example");

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);
  staticRouting->SetDefaultRoute (gateway, interface);


  Ptr<FdNetDevice> dev = DynamicCast<FdNetDevice> (device);
  if (dev)
    {
      Simulator::Schedule (Seconds (5), &Send, dev, mode);
    }

  NS_LOG_INFO ("Run Emulation.");
  Simulator::Stop (Seconds (7.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
