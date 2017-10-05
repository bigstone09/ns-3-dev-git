Netmap NetDevice
-------------------------
.. include:: replace.txt
.. highlight:: cpp

.. heading hierarchy:
   ------------- Chapter
   ************* Section (#.#)
   ============= Subsection (#.#.#)
   ############# Paragraph (no number)

The ``fd-net-device`` module provides the ``NetmapNetDevice`` class, a class derived
from the ``FdNetDevice`` which is able to read and write traffic using a netmap file descriptor
provided by the user. This file descriptor must be associated to a real ethernet device
in the host machine.
netmap is a fast packets processing that bypasses the host networking stack and gains
direct access to network device.

The ``EmuFdNetDeviceHelper`` provides the method ``SetNetmapMode`` to enable
the emulation of a real device in netmap mode.

This device supports the use of netmap in both generic (or emulated) mode and in native mode.

Model Description
*****************

The NetmapNetDevice is a special type of |ns3| NetDevice that reads traffic
to and from a netmap file descriptor. The file descriptor must be associated
to a real ethernet device in the host machine.

This device requires a netmap installation in the host machine for the compilation
phase. Also, the user must load the netmap module before to use this device.

The NetmapNetDevice extends the FdNetDevice design by specializing the write and read operations.
The operation of write copy a packet from |ns3| to the netmap transmission ring. The device
syncs the netmap transmission ring periodically. The read operation
copy a packet from the netmap receiver ring to |ns3|. The device syncs the netmap receiver ring after
each packet copied.

This device provides support for flow-control by means a separate thread. The device use
a NetDeviceQueue interface to represent the netmap transmission ring. In the operation of write, after
the packet copy in the transmission ring, if there is no room for other packets the device stops the
queue and notify a separate thread about this event. The separate thread waits for the next slot available
for transmission and when there is room for other packets wake the queue. In meanwhile, the main process
run separately.

The netmap device provides support for queue limits in the netmap ring by periodic notification to queue
limits about the transmitted bytes.

Requirments
************

This device requires a netmap installation in the host machine. The user can follow the
tutorial provided by netmap or the wiki page related to this device to download, to configure and
to install netmap in the host machine.

After the installation of netmap, supports for this device can be found in the output of the ``waf configure``:

.. sourcecode:: text

   Netmap Emulated FdNetDevice : enabled

The user must set the right privileges for the creator program by enabling the ``--enable-sudo``
flag when performing ``waf configure``.

When the user use netmap in emulated mode, it may disable the generic_txqdisc by setting to 0 this
value of netmap (By default netmap in emulated mode introduce a netmap aware queue disc on the netmap switched device).
Also, the user can reduce the generic_ringsize value of netmap (netmap uses a default value of 1024 when it is unable
to read the real tx ring from the device, i.e. when the device do not supports ethtool).

Finally, when the user use netmap in native mode, it must compile and load the netmap aware device driver. The user can follow
the netmap doc page or the wiki page related to this device to compile and load the netmap aware device driver.

Usage
*****

After the configuration, the user must load the netmap module in the host machine before
to use this device, e.g. by the insmod of the netmap kernel module generated with the netmap compilation.

The NetmapNetDevice will be used to interact with the host system and the user can follow the typical
usage pattern for the FdNetDevice in such cases. For instance, the user sets the netmap emulation mode on the
FdNetDeviceEmuHelper, sets the real time simulation and enables the checksum calculaction:

::

  EmuFdNetDeviceHelper emu;
  emu.SetDeviceName (deviceName);
  emu.SetNetmapMode ();
  ...
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

The real device interface must be in promiscuous mode and the IP of the emulated device must be different from
the real OS IP for that device but on the same subnet.

Examples
********

Four examples are provided about the NetmapNetDevice:

* ``fd-netmap-emu-ping.cc``:   This example is aimed at providing a test of this device on a real ethernet device.
  This example sends ICMP traffic over a real device emulated in netmap mode with UDP background traffic.

* ``fd-netmap-emu-onoff.cc``: This example is aimed at measuring the throughput of the
  NetmapNetDevice connected back-to-back to a simulated (with this example) UDP or TCP server or real application
  UDP or TCP server (e.g. iperf).

* ``fd-netmap-emu-send.cc``:  This example is aimed at measuring the maximum transmission rate in pps achievable
  with the NetmapNetDevice.

* ``fd-netmap-emu-tc.cc``:  This example build a node with two interface in netmap mode.
  The user can explore different qdiscs behaviours on the backlog of a device emulated with netmap.

Performance evaluation
**********************

Throughput and rtt
==================

This device was evaluated by comparing the behavior of the NetmapNetDevice with the FdNetDevice in emulated mode on a back-to-back through
ethernet. We used the ``fd-netmap-emu-onoff.cc`` example to perform this evaluation. and the ns-3 code compiled in optimized mode.

We used two hosts equipped with Intel i7 at 3.8 GHz cpu and Intel e1000e 1 Gbps ethernet card. The two host was connected by a cross ethernet cable.
Clint side, the OS IP for the emulated device was 10.0.1.1 while the ns-3 IP was 10.0.1.11, the interface was in promisc mode and we used netmap in emulated mode
(i.e., without driver changes) with its generic_txqdisc=0 (to keep out the queue disc netmap aware introduced by netmap in emulated mode).
Server side the OS IP was 10.0.1.2.
The server at specified address had an instance of an UDP server (e.g., an instance of iperf in UDP server mode with "iperf -s -p 8000 -u") and an estimation
of the received throughput could be performed by Wireshark in capture mode on the server interface.

The user can run the example client side by:

./waf --run "src/fd-net-device/examples/fd-netmap-emu-onoff --netmapMode=true --ping=true --transportProt='Udp' --server='10.0.1.2' --bql=false"

to evaluate the NetmapNetDevice.

The user can perform the evaluation in the same scenario with --netmapMode=false to evaluate the FdNetDevice in emulation mode.
The example output will be the ping reponse, the backlog in traffic-control qdisc and the bytes inflight in the netmap transmission ring (in case of emulation with netmap).
Finally, in case of emulation in netmap mode, the user can explore the use of BQL with --bql=true and observes a reduction of the bytes in flight in the
netmap transmission ring.

The throughput performance of this device is very similar to the socket one (i.e., the FdNetDevice configured in emulation mode).
We evaluated a received UDP throughput up to 700 Mbps on a 1 Gbps link with netmap in emulated (and in native) mode with a packet size of 1400 bytes.
In case of TCP the throughput performance is lower than the UDP case and it is of about 500 Mbps.
We evaluated with oprofile a performance bottleneck in the modules IP and TCP of ns-3.

Conversely, the delay performance with netmap is different from the socket one. Indeed, the delay evaluated by means the ping response with netmap
is smaller than the socket one. This is due to a more realistic queues occupancy with netmap compared with the higher
socket buffer (where the delay grows in an uncontrolled manner). Further optimizations can leads to more stable delay with netmap.

Finally, in case of link saturation, e.g. in case of 100 Mbps ethernet link, the support for flow control and queue limits for this device
allows keeping a backlog in traffic-control where the packets can be managed by advanced queue discs. The user can explore different qdiscs
behaviors with netmap emulated devices.

Pps
===

A further evaluation regards the maximum transmission rate achievable with the NetmapNetDevice in pps.
We used the ``fd-netmap-emu-send.cc`` example to perform this evaluation and the ns-3 code compiled in optimized mode.

We used the same hosts configuration of the previous evaluation without iperf server side.

The user can run the example with

./waf --run "src/fd-net-device/examples/fd-netmap-emu-send --netmapMode=true --mode=0"

to evaluate the maximum pps achievable with netmap with the lowest primitive (i.e., the Write method).

The user can perform the evaluation in the same scenario with --netmapMode=false to evaluate the FdNetDevice in emulation mode with the lowest
primitive (i.e., the socket write).
The example output will be the number of packets sent in 1 s, the actual period of measure in ms, the sent failed, and the estimated throughput.

The pps achievable with the NetmapNetDevice was up to 1.38 Mpps on the e1000e adapter on the i7 cpu at full frequency of 3.8 GHZ.
This value is according to netmap results on the Intel e1000e 1 Gbps adapter available in the netmap documentation.
The pps provide by socket (i.e., FdNetDevice in emulated mode) is in line to the netmap one on this testbed at full cpu frequency.

We explored the reduction of the cpu frequency to better present the gain provided by netmap on 1 Gbps link.
With cpu frequency of 2.5 GHz (set with cpufrequtil) we observed a gain in the pps with netmap of about 10% (netmap achieves 1.38 Mpps while
the socket achieves 1.25 Mpps). A further reduction to 2 GHz leads to a gain of about 30 % with netmap (netmap achieves 1.38 Mpps while
the socket achieves 1.01 Mpps).
