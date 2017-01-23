.. include:: replace.txt
.. highlight:: cpp
.. highlight:: bash

Mq queue disc
------------------

This chapter describes the mq queue disc implementation in |ns3|.

The mq is a classful multiqueue dummy scheduler developed to best fit the multiqueue
traffic control API in Linux. The mq scheduler present device transmission queues as
classes, allowing to attach different queue discs to them, which are grafted to the
device transmission queues.

Model Description
*****************

The source code for the mq queue disc is located in the directory
``src/traffic-control/model`` and consists of 2 files `mq-queue-disc.h`
and `mq-queue-disc.cc` defining a MqQueueDisc class. The code was ported to |ns3| based
on Linux kernel code.

* class :cpp:class:`MqQueueDisc`: This class implements the mq dummy scheduler:

  * ``MqQueueDisc::InitializeParams ()``: This routine sets the queue disc wake mode to WAKE_CHILD to graft each class to their device queue.

  * ``MqQueueDisc::DoEnqueue ()``: This function do not perform any operation. The packets are enqueued in the queue disc grafted to the device queue are destined to.

  * ``MqQueueDisc::DoDequeue ()``: This function do not perform any operation. The packets are dequeued from the queue disc grafted to the device queue are destined to.

In Linux, by default, the mq scheduler is used for a multiqueue device, e.g. a QoS WiFi device.
It instances a system default queue disc for each device queue, e.g. a pfifo_fast queue disc.
The user can replace the default configuration by graft other queue disc configuration for
each class.
In |ns3|, the mq with pfifo_fast classes is a default configuration for a multiqueue device.
The user can specify a desidered type for each class of a mq queue disc by traffic control helper.
Finally, neither internal filters nor queues can be configured for an mq queue disc.

Examples
========

A typical usage pattern is to create a traffic control helper and to configure the internal classes
of mq from the helper. For example, mq with classes of type FqCodel can be configured as follows:

.. sourcecode:: cpp

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::MqQueueDisc");
  TrafficControlHelper::ClassIdList cls = tch.AddQueueDiscClasses (handle, numTxQueues, "ns3::QueueDiscClass");
  TrafficControlHelper::HandleList hdl = tch.AddChildQueueDiscs (handle, cls, "ns3::FqCoDelQueueDisc");
  tch.AddPacketFilters (hdl, "ns3::FqCoDelIpv4PacketFilter");

  QueueDiscContainer qdiscs = tch.Install (devices);
