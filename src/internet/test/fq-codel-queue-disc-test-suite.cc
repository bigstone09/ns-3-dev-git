/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 Universita' degli Studi di Napoli Federico II
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

#include "ns3/test.h"
#include "ns3/fq-codel-queue-disc.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-packet-filter.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-header.h"
#include "ns3/ipv6-queue-disc-item.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"

using namespace ns3;

/**
 * This class tests non-IP packets
 */
class FQCoDelQueueDiscNonIP : public TestCase
{
public:
  FQCoDelQueueDiscNonIP ();
  virtual ~FQCoDelQueueDiscNonIP ();

private:
  virtual void DoRun (void);
};

FQCoDelQueueDiscNonIP::FQCoDelQueueDiscNonIP ()
  : TestCase ("Test non-IP packets")
{
}

FQCoDelQueueDiscNonIP::~FQCoDelQueueDiscNonIP ()
{
}

void
FQCoDelQueueDiscNonIP::DoRun (void)
{
  // Packets with non-IP headers should enqueue into the same class
  Ptr<FQCoDelQueueDisc> queueDisc = CreateObjectWithAttributes<FQCoDelQueueDisc> ("Packet limit", UintegerValue (4));
  Ptr<FQCoDelIpv4PacketFilter> filter = CreateObject<FQCoDelIpv4PacketFilter> ();
  queueDisc->AddPacketFilter (filter);

  queueDisc->Initialize ();

  Ptr<Packet> p;
  p = Create<Packet> ();
  Ptr<Ipv6QueueDiscItem> item;
  Ipv6Header ipv6Header;
  Address dest;
  item = Create<Ipv6QueueDiscItem> (p, dest, 0, ipv6Header);
  queueDisc->Enqueue (item);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue depth");

  p = Create<Packet> (reinterpret_cast<const uint8_t*> ("hello, world"), 12);
  item = Create<Ipv6QueueDiscItem> (p, dest, 0, ipv6Header);
  queueDisc->Enqueue (item);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue depth");
}

/**
 * This class tests the IP flows separation and the packet limit 
 */
class FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit : public TestCase
{
public:
  FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit ();
  virtual ~FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit ();

private:
  virtual void DoRun (void);
  void AddPacket (Ptr<FQCoDelQueueDisc> queue, Ipv4Header hdr);
};

FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit::FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit ()
  : TestCase ("Test IP flows separation and packet limit")
{
}

FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit::~FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit ()
{
}

void
FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit::AddPacket (Ptr<FQCoDelQueueDisc> queue, Ipv4Header hdr)
{
  Ptr<Packet> p = Create<Packet> (100);
  Address dest;
  Ptr<Ipv4QueueDiscItem> item = Create<Ipv4QueueDiscItem> (p, dest, 0, hdr);
  queue->Enqueue (item);
}

void
FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit::DoRun (void)
{
  Ptr<FQCoDelQueueDisc> queueDisc = CreateObjectWithAttributes<FQCoDelQueueDisc> ("Packet limit", UintegerValue (4));
  Ptr<FQCoDelIpv4PacketFilter> filter = CreateObject<FQCoDelIpv4PacketFilter> ();
  queueDisc->AddPacketFilter (filter);
  
  queueDisc->Initialize ();

  Ipv4Header hdr;
  hdr.SetPayloadSize (100);
  hdr.SetSource (Ipv4Address ("10.10.1.1"));
  hdr.SetDestination (Ipv4Address ("10.10.1.2"));
  hdr.SetProtocol (7);

  // Add three packets from the first flow
  AddPacket (queueDisc, hdr);
  AddPacket (queueDisc, hdr);
  AddPacket (queueDisc, hdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 3, "unexpected queue disc depth");
  
  // Add two packets from the second flow
  hdr.SetDestination (Ipv4Address ("10.10.1.7"));
  // Add the first packet
  AddPacket (queueDisc, hdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 4, "unexpected queue disc depth");
  // Add the second packet that causes a drop from the fat flow
  AddPacket (queueDisc, hdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 4, "unexpected queue disc depth");
}

/**
 * This class tests the credits per flow
 */
class FQCoDelQueueDiscCredits : public TestCase
{
public:
  FQCoDelQueueDiscCredits ();
  virtual ~FQCoDelQueueDiscCredits ();

private:
  virtual void DoRun (void);
  void AddPacket (Ptr<FQCoDelQueueDisc> queue, Ipv4Header hdr);
};

FQCoDelQueueDiscCredits::FQCoDelQueueDiscCredits ()
  : TestCase ("Test credits and flows status")
{
}

FQCoDelQueueDiscCredits::~FQCoDelQueueDiscCredits ()
{
}

void
FQCoDelQueueDiscCredits::AddPacket (Ptr<FQCoDelQueueDisc> queue, Ipv4Header hdr)
{
  Ptr<Packet> p = Create<Packet> (100);
  Address dest;
  Ptr<Ipv4QueueDiscItem> item = Create<Ipv4QueueDiscItem> (p, dest, 0, hdr);
  queue->Enqueue (item);
}

void
FQCoDelQueueDiscCredits::DoRun (void)
{
  Ptr<FQCoDelQueueDisc> queueDisc = CreateObjectWithAttributes<FQCoDelQueueDisc> ("Quantum", UintegerValue (90));
  Ptr<FQCoDelIpv4PacketFilter> filter = CreateObject<FQCoDelIpv4PacketFilter> ();
  queueDisc->AddPacketFilter (filter);
  
  queueDisc->Initialize ();

  Ipv4Header hdr;
  hdr.SetPayloadSize (100);
  hdr.SetSource (Ipv4Address ("10.10.1.1"));
  hdr.SetDestination (Ipv4Address ("10.10.1.2"));
  hdr.SetProtocol (7);
  
  // Add a packet from the first flow
  AddPacket (queueDisc, hdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 1, "unexpected queue disc depth");
  // Dequeue a packet (the number of credits for the first flow now are 90 - 100 = -10)
  queueDisc->Dequeue ();
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 0, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 0, "unexpected queue disc depth");
  Ptr<FQCoDelFlow> flow1 = StaticCast<FQCoDelFlow> (queueDisc->GetQueueDiscClass (0));

  // Add two packets from the first flow
  AddPacket (queueDisc, hdr);
  AddPacket (queueDisc, hdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 2, "unexpected queue disc depth");
  
  // Add two packets from the second flow
  hdr.SetDestination (Ipv4Address ("10.10.1.10"));
  AddPacket (queueDisc, hdr);
  AddPacket (queueDisc, hdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 4, "unexpected queue disc depth");

  // Dequeue from the second flow (the first flow has a negative number of credits)
  queueDisc->Dequeue ();
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 3, "unexpected queue disc depth");

  // Dequeue from the first flow
  queueDisc->Dequeue ();
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 2, "unexpected queue disc depth");

  // Dequeue from the second flow
  queueDisc->Dequeue ();
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 0, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 1, "unexpected queue disc depth");

  // Dequeue from the first flow
  queueDisc->Dequeue ();
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 0, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 0, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 0, "unexpected queue disc depth");
}

/**
 * This class tests the TCP flows separation
 */
class FQCoDelQueueDiscTCPFlowsSeparation : public TestCase
{
public:
  FQCoDelQueueDiscTCPFlowsSeparation ();
  virtual ~FQCoDelQueueDiscTCPFlowsSeparation ();

private:
  virtual void DoRun (void);
  void AddPacket (Ptr<FQCoDelQueueDisc> queue, Ipv4Header ipHdr, TcpHeader tcpHdr);
};

FQCoDelQueueDiscTCPFlowsSeparation::FQCoDelQueueDiscTCPFlowsSeparation ()
  : TestCase ("Test TCP flows separation")
{
}

FQCoDelQueueDiscTCPFlowsSeparation::~FQCoDelQueueDiscTCPFlowsSeparation ()
{
}

void
FQCoDelQueueDiscTCPFlowsSeparation::AddPacket (Ptr<FQCoDelQueueDisc> queue, Ipv4Header ipHdr, TcpHeader tcpHdr)
{
  Ptr<Packet> p = Create<Packet> (100);
  p->AddHeader (tcpHdr);
  Address dest;
  Ptr<Ipv4QueueDiscItem> item = Create<Ipv4QueueDiscItem> (p, dest, 0, ipHdr);
  queue->Enqueue (item);
}

void
FQCoDelQueueDiscTCPFlowsSeparation::DoRun (void)
{
  Ptr<FQCoDelQueueDisc> queueDisc = CreateObjectWithAttributes<FQCoDelQueueDisc> ("Packet limit", UintegerValue (10));
  Ptr<FQCoDelIpv4PacketFilter> filter = CreateObject<FQCoDelIpv4PacketFilter> ();
  queueDisc->AddPacketFilter (filter);
  
  queueDisc->Initialize ();

  Ipv4Header hdr;
  hdr.SetPayloadSize (100);
  hdr.SetSource (Ipv4Address ("10.10.1.1"));
  hdr.SetDestination (Ipv4Address ("10.10.1.2"));
  hdr.SetProtocol (6);
  
  TcpHeader tcpHdr;
  tcpHdr.SetSourcePort (7);
  tcpHdr.SetDestinationPort (27);

  // Add three packets from the first flow
  AddPacket (queueDisc, hdr, tcpHdr);
  AddPacket (queueDisc, hdr, tcpHdr);
  AddPacket (queueDisc, hdr, tcpHdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 3, "unexpected queue disc depth");
  
  // Add a packet from the second flow
  tcpHdr.SetSourcePort (8);
  AddPacket (queueDisc, hdr, tcpHdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 4, "unexpected queue disc depth");

  // Add a packet from the third flow
  tcpHdr.SetDestinationPort (28);
  AddPacket (queueDisc, hdr, tcpHdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (2)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 5, "unexpected queue disc depth");

  // Add two packets from the fourth flow
  tcpHdr.SetSourcePort (7);
  AddPacket (queueDisc, hdr, tcpHdr);
  AddPacket (queueDisc, hdr, tcpHdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (2)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (3)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 7, "unexpected queue disc depth");
}

/**
 * This class tests the UDP flows separation
 */
class FQCoDelQueueDiscUDPFlowsSeparation : public TestCase
{
public:
  FQCoDelQueueDiscUDPFlowsSeparation ();
  virtual ~FQCoDelQueueDiscUDPFlowsSeparation ();

private:
  virtual void DoRun (void);
  void AddPacket (Ptr<FQCoDelQueueDisc> queue, Ipv4Header ipHdr, UdpHeader udpHdr);
};

FQCoDelQueueDiscUDPFlowsSeparation::FQCoDelQueueDiscUDPFlowsSeparation ()
  : TestCase ("Test UDP flows separation")
{
}

FQCoDelQueueDiscUDPFlowsSeparation::~FQCoDelQueueDiscUDPFlowsSeparation ()
{
}

void
FQCoDelQueueDiscUDPFlowsSeparation::AddPacket (Ptr<FQCoDelQueueDisc> queue, Ipv4Header ipHdr, UdpHeader udpHdr)
{
  Ptr<Packet> p = Create<Packet> (100);
  p->AddHeader (udpHdr);
  Address dest;
  Ptr<Ipv4QueueDiscItem> item = Create<Ipv4QueueDiscItem> (p, dest, 0, ipHdr);
  queue->Enqueue (item);
}

void
FQCoDelQueueDiscUDPFlowsSeparation::DoRun (void)
{
  Ptr<FQCoDelQueueDisc> queueDisc = CreateObjectWithAttributes<FQCoDelQueueDisc> ("Packet limit", UintegerValue (10));
  Ptr<FQCoDelIpv4PacketFilter> filter = CreateObject<FQCoDelIpv4PacketFilter> ();
  queueDisc->AddPacketFilter (filter);
  
  queueDisc->Initialize ();

  Ipv4Header hdr;
  hdr.SetPayloadSize (100);
  hdr.SetSource (Ipv4Address ("10.10.1.1"));
  hdr.SetDestination (Ipv4Address ("10.10.1.2"));
  hdr.SetProtocol (17);
  
  UdpHeader udpHdr;
  udpHdr.SetSourcePort (7);
  udpHdr.SetDestinationPort (27);

  // Add three packets from the first flow
  AddPacket (queueDisc, hdr, udpHdr);
  AddPacket (queueDisc, hdr, udpHdr);
  AddPacket (queueDisc, hdr, udpHdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 3, "unexpected queue disc depth");

  // Add a packet from the second flow
  udpHdr.SetSourcePort (8);
  AddPacket (queueDisc, hdr, udpHdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 4, "unexpected queue disc depth");

  // Add a packet from the third flow
  udpHdr.SetDestinationPort (28);
  AddPacket (queueDisc, hdr, udpHdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (2)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 5, "unexpected queue disc depth");

  // Add two packets from the fourth flow
  udpHdr.SetSourcePort (7);
  AddPacket (queueDisc, hdr, udpHdr);
  AddPacket (queueDisc, hdr, udpHdr);
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (0)->GetQueueDisc ()->GetNPackets (), 3, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (1)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (2)->GetQueueDisc ()->GetNPackets (), 1, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->GetQueueDiscClass (3)->GetQueueDisc ()->GetNPackets (), 2, "unexpected queue disc depth");
  NS_TEST_ASSERT_MSG_EQ (queueDisc->QueueDisc::GetNPackets (), 7, "unexpected queue disc depth");
}

class FQCoDelQueueDiscTestSuite : public TestSuite
{
public:
  FQCoDelQueueDiscTestSuite ();
};

FQCoDelQueueDiscTestSuite::FQCoDelQueueDiscTestSuite ()
  : TestSuite ("fq-codel-queue-disc", UNIT)
{
  AddTestCase (new FQCoDelQueueDiscNonIP, TestCase::QUICK);
  AddTestCase (new FQCoDelQueueDiscIPFlowsSeparationAndPacketLimit, TestCase::QUICK);
  AddTestCase (new FQCoDelQueueDiscCredits, TestCase::QUICK);
  AddTestCase (new FQCoDelQueueDiscTCPFlowsSeparation, TestCase::QUICK);
  AddTestCase (new FQCoDelQueueDiscUDPFlowsSeparation, TestCase::QUICK);
}

static FQCoDelQueueDiscTestSuite pfifoFastQueueTestSuite;
