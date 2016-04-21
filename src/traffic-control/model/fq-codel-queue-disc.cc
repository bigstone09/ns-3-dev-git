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

#include "ns3/log.h"
#include "ns3/string.h"
#include "fq-codel-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FQCoDelQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (FQCoDelFlow);

TypeId FQCoDelFlow::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FQCoDelFlow")
    .SetParent<QueueDiscClass> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<FQCoDelFlow> ()
  ;
  return tid;
}

FQCoDelFlow::FQCoDelFlow ()
  : m_credits (0),
    m_active (false)
{
  NS_LOG_FUNCTION (this);
}

FQCoDelFlow::~FQCoDelFlow ()
{
  NS_LOG_FUNCTION (this);
}

void
FQCoDelFlow::SetCredits (int32_t credits)
{
  NS_LOG_FUNCTION (this << credits);
  m_credits = credits;
}

int32_t
FQCoDelFlow::GetCredits ()
{
  NS_LOG_FUNCTION (this);
  return m_credits;
}

void
FQCoDelFlow::AddCredits (int32_t credits)
{
  NS_LOG_FUNCTION (this << credits);
  m_credits += credits;
}

void
FQCoDelFlow::SubCredits (int32_t credits)
{
  NS_LOG_FUNCTION (this << credits);
  m_credits -= credits;
}

void
FQCoDelFlow::SetActive (bool active)
{
  NS_LOG_FUNCTION (this);
  m_active = active;
}

bool
FQCoDelFlow::GetActive ()
{
  NS_LOG_FUNCTION (this);
  return m_active;
}


NS_OBJECT_ENSURE_REGISTERED (FQCoDelQueueDisc);

TypeId FQCoDelQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FQCoDelQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<FQCoDelQueueDisc> ()
    .AddAttribute ("Interval",
                   "The CoDel algorithm interval for each FQCoDel queue",
                   StringValue ("100ms"),
                   MakeStringAccessor (&FQCoDelQueueDisc::m_interval),
                   MakeStringChecker ())
    .AddAttribute ("Target",
                   "The CoDel algorithm target queue delay for each FQCoDel queue",
                   StringValue ("5ms"),
                   MakeStringAccessor (&FQCoDelQueueDisc::m_target),
                   MakeStringChecker ())
    .AddAttribute ("Packet limit",
                   "The hard limit on the real queue size, measured in packets",
                   UintegerValue (10 * 1024),
                   MakeUintegerAccessor (&FQCoDelQueueDisc::m_limit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Quantum",
                   "The number of bytes each queue gets to dequeue on each round of the scheduling algorithm",
                   UintegerValue (1514),
                   MakeUintegerAccessor (&FQCoDelQueueDisc::m_quantum),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Flows",
                   "The number of queues into which the incoming packets are classified",
                   UintegerValue (1024),
                   MakeUintegerAccessor (&FQCoDelQueueDisc::m_flows),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

FQCoDelQueueDisc::FQCoDelQueueDisc ()
  : m_overlimitDroppedPackets (0)
{
  NS_LOG_FUNCTION (this);
}

FQCoDelQueueDisc::~FQCoDelQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

bool
FQCoDelQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  int32_t ret = Classify (item);

  if (ret == PacketFilter::PF_NO_MATCH)
    {
      NS_LOG_ERROR ("No filter has been able to classify this packet. Likely, "
                    "a filter for this type of packets have not been installed ");
    }

  uint32_t h = ret % m_flows;

  Ptr<FQCoDelFlow> flow;
  if (m_flowsIndices.find (h) == m_flowsIndices.end ())
    {
      flow = m_flowFactory.Create<FQCoDelFlow> ();
      Ptr<QueueDisc> qd = m_queueDiscFactory.Create<QueueDisc> ();
      qd->Initialize ();
      flow->SetQueueDisc (qd);
      AddQueueDiscClass (flow);

      m_flowsIndices[h] = GetNQueueDiscClasses () - 1;
    }
  else
    {
      flow = StaticCast<FQCoDelFlow> (GetQueueDiscClass (m_flowsIndices[h]));
    }

  if (!flow->GetActive ())
    {
      flow->SetActive (true);
      flow->SetCredits (m_quantum);
      m_newFlows.push_back (flow);
    }

  flow->GetQueueDisc ()->Enqueue (item);

  NS_LOG_DEBUG ("Packet enqueued into flow " << h << "; flow index " << m_flowsIndices[h]);

  if (GetNPackets () > m_limit)
    {
      FQCoDelDrop ();
    }

  return true;
}

Ptr<QueueDiscItem>
FQCoDelQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<FQCoDelFlow> flow;
  Ptr<QueueDiscItem> item;

  do
    {
      bool found = false;

      while (!found && !m_newFlows.empty ())
        {
          flow = m_newFlows.front ();

          if (flow->GetCredits () <= 0)
            {
              flow->AddCredits (m_quantum);
              m_oldFlows.push_back (flow);
              m_newFlows.pop_front ();
            }
          else
            {
              found = true;
            }
        }

      while (!found && !m_oldFlows.empty ())
        {
          flow = m_oldFlows.front ();

          if (flow->GetCredits () <= 0)
            {
              flow->AddCredits (m_quantum);
              m_oldFlows.push_back (flow);
              m_oldFlows.pop_front ();
            }
          else
            {
              found = true;
            }
        }

      if (!found)
        {
          return 0;
        }

      item = flow->GetQueueDisc ()->Dequeue ();

      if (!item)
        {
          if (!m_newFlows.empty ())
            {
              m_oldFlows.push_back (flow);
              m_newFlows.pop_front ();
            }
          else
            {
              flow->SetActive (false);
              m_oldFlows.pop_front ();
            }
        }
    } while (item == 0);

  flow->SubCredits (item->GetPacketSize ());

  return item;
}

Ptr<const QueueDiscItem>
FQCoDelQueueDisc::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  Ptr<FQCoDelFlow> flow;

  if (!m_newFlows.empty ())
    {
      flow = m_newFlows.front ();
    }
  else
    {
      if (!m_oldFlows.empty ())
        {
          flow = m_oldFlows.front ();
        }
      else
        {
          return 0;
        }
    }

  return flow->GetQueueDisc ()->Peek ();
}

bool
FQCoDelQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("FQCoDelQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () == 0)
    {
      NS_LOG_ERROR ("FQCoDelQueueDisc needs at least a packet filter");
      return false;
    }

  if (GetNInternalQueues () > 0)
    {
      NS_LOG_ERROR ("FQCoDelQueueDisc cannot have internal queues");
      return false;
    }

  return true;
}

void
FQCoDelQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);

  m_flowFactory.SetTypeId ("ns3::FQCoDelFlow");

  m_queueDiscFactory.SetTypeId ("ns3::CoDelQueueDisc");
  m_queueDiscFactory.Set ("Mode", EnumValue (Queue::QUEUE_MODE_PACKETS));
  m_queueDiscFactory.Set ("MaxPackets", UintegerValue (m_limit + 1));
  m_queueDiscFactory.Set ("Interval", StringValue (m_interval));
  m_queueDiscFactory.Set ("Target", StringValue (m_target));
}

uint32_t
FQCoDelQueueDisc::FQCoDelDrop ()
{
  NS_LOG_FUNCTION (this);

  uint32_t maxBacklog = 0, index = 0;
  Ptr<QueueDisc> qd;

  /* Queue is full! Find the fat flow and drop packet from it */
  for (uint32_t i = 0; i < GetNQueueDiscClasses (); i++)
    {
      qd = GetQueueDiscClass (i)->GetQueueDisc ();
      uint32_t bytes = qd->GetNBytes ();
      if (bytes > maxBacklog)
        {
          maxBacklog = bytes;
          index = i;
        }
    }

  qd = GetQueueDiscClass (index)->GetQueueDisc ();
  qd->GetInternalQueue (0)->Remove ();
  m_overlimitDroppedPackets++;

  return index;
}

} // namespace ns3
