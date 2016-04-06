/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

/* #include <limits> */
#include "ns3/log.h"
/* #include "ns3/enum.h" */
/* #include "ns3/uinteger.h" */
#include "fq-codel-queue-disc.h"
/* #include "ns3/red-queue.h" */
/* #include "ns3/ipv4-header.h" */
/* #include "ns3/ppp-header.h" */
/* #include <boost/functional/hash.hpp> */
/* #include <boost/format.hpp> */

/*
 * FQ_Codel as implemented by Linux.
 */

NS_LOG_COMPONENT_DEFINE ("Fq_CoDelQueue");

/* using namespace boost; */

namespace ns3 {

Fq_CoDelSlot::Fq_CoDelSlot () :
  h(0)
{
  NS_LOG_FUNCTION (this);
  /* INIT_LIST_HEAD(&flowchain); */
  q = CreateObject<CoDelQueueDisc> ();
  q->Initialize (); // to check config and create the internal queue
}

Fq_CoDelSlot::~Fq_CoDelSlot ()
{
  NS_LOG_FUNCTION (this);
}

NS_OBJECT_ENSURE_REGISTERED (Fq_CoDelQueue);

TypeId Fq_CoDelQueue::GetTypeId (void) 
{
  static TypeId tid = TypeId ("ns3::Fq_CoDelQueue")
    .SetParent<QueueDisc> ()
    .AddConstructor<Fq_CoDelQueue> ()
    .AddAttribute ("headMode",
                   "Add new flows in the head position",
                   BooleanValue (false),
                   MakeBooleanAccessor (&Fq_CoDelQueue::m_headmode),
                   MakeBooleanChecker ())
    .AddAttribute ("peturbInterval",
                   "Peterbation interval in packets",
                   UintegerValue (500000),
                   MakeUintegerAccessor (&Fq_CoDelQueue::m_peturbInterval),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Quantum",
                   "Quantum in bytes",
                   UintegerValue (1500),
                   MakeUintegerAccessor (&Fq_CoDelQueue::m_quantum),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Flows",
                   "Number of flows",
                   UintegerValue (1024),
                   MakeUintegerAccessor (&Fq_CoDelQueue::m_divisor),
                   MakeUintegerChecker<uint32_t> ())
    ;
  return tid;
}

Fq_CoDelQueue::Fq_CoDelQueue () :
  m_ht (),
  psource ()
  /* peturbation (psource.GetInteger(0,std::numeric_limits<std::size_t>::max())) */ ///\todo SIGSEGV
{
  NS_LOG_FUNCTION (this);
  /* INIT_LIST_HEAD(&m_new_flows); */
  /* INIT_LIST_HEAD(&m_old_flows); */
}

Fq_CoDelQueue::~Fq_CoDelQueue ()
{
  NS_LOG_FUNCTION (this);
}

/* std::size_t
Fq_CoDelQueue::hash(Ptr<Packet> p)
{
  boost::hash<std::string> string_hash;

  Ptr<Packet> q = p->Copy();

  class PppHeader ppp_hd;

  q->RemoveHeader(ppp_hd);

  class Ipv4Header ip_hd;
  if (q->PeekHeader (ip_hd))
    {
      if (pcounter > m_peturbInterval)
        peturbation = psource.GetInteger(0,std::numeric_limits<std::size_t>::max());
      std::size_t h = (string_hash((format("%x%x%x%x")
                                    % (ip_hd.GetDestination().Get())
                                    % (ip_hd.GetSource().Get())
                                    % (ip_hd.GetProtocol())
                                    % (peturbation)).str())
                       % m_divisor);
      return h;
    }
  else
    {
      return 0;
    }
} */

bool 
Fq_CoDelQueue::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  bool queued;

  /* Fq_CoDelSlot *slot; */
  Ptr<Fq_CoDelSlot> slot;

  /* std::size_t h = Fq_CoDelQueue::hash(p);
  NS_LOG_DEBUG ("fq_codel enqueue use queue "<<h); */
  uint32_t h = Classify (item) % m_divisor;
  NS_LOG_DEBUG ("Enqueues into slot " << h);

  bool slotInFlows = false;

  if (m_ht[h] == NULL)
    {
      NS_LOG_DEBUG ("fq_codel enqueue Create queue " << h);
      m_ht[h] = new Fq_CoDelSlot ();
      slot = m_ht[h];
      /* slot->q->backlog = &backlog; */
      slot->h = h;
    } 
  else 
    {
      slot = m_ht[h];
      slotInFlows = true;
    }

  queued = slot->q->Enqueue(item);

  if (queued)
    {
      slot->backlog += item->GetPacketSize();
      backlog += item->GetPacketSize();

      /* if (list_empty(&slot->flowchain)) {
        NS_LOG_DEBUG ("fq_codel enqueue inactive queue "<<h);
        list_add_tail(&slot->flowchain, &m_new_flows); // add the slot->flowchain to the m_new_flows
        slot->deficit = m_quantum;
      } */
      if (!slotInFlows)
        {
	  NS_LOG_DEBUG ("fq_codel enqueue inactive queue "<<h);
	  slot->deficit = m_quantum;
	  m_newFlows.push_back (slot);
        }
    }
  else
    {
      Drop (item);
    }
  NS_LOG_DEBUG ("fq_codel enqueue "<<slot->h<<" "<<queued);
  return queued;
}

Ptr<QueueDiscItem>
Fq_CoDelQueue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);
  /* Fq_CoDelSlot *flow; */
  /* struct list_head *head; */

  Ptr<Fq_CoDelSlot> flow;

begin:
  /* head = &m_new_flows;
  if (list_empty(head)) {
    head = &m_old_flows;
    if (list_empty(head))
      return NULL;
  } */

  /* flow = list_first_entry(head, Fq_CoDelSlot, flowchain); // offsetof_hack */

  if (!m_newFlows.empty ())
    {
      flow = m_newFlows.front ();
    }
  else if (!m_oldFlows.empty ())
    {
      flow = m_oldFlows.front ();
    }
  else
    {
      return 0;
    }

  NS_LOG_DEBUG ("fq_codel scan "<<flow->h);

  if (flow->deficit <= 0) 
    {
      flow->deficit += m_quantum;
      NS_LOG_DEBUG ("fq_codel deficit now "<<flow->deficit<<" "<<flow->h);
      /* list_move_tail(&flow->flowchain, &m_old_flows); // move flow->flowchain to m_old_flows */
      if (!m_newFlows.empty ())
        {
	  m_oldFlows.push_back (m_newFlows.front ());
	  m_newFlows.pop_front ();
	}
      else
        {
	  m_oldFlows.push_back (m_oldFlows.front ());
	  m_oldFlows.pop_front ();
	}
      goto begin;
    }

  Ptr<QueueDiscItem> item = flow->q->Dequeue();
  if (item == NULL)
    {
      /* force a pass through old_flows to prevent starvation */
      /* if ((head == &m_new_flows) && !list_empty(&m_old_flows))
        list_move_tail(&flow->flowchain, &m_old_flows);
      else
        list_del_init(&flow->flowchain); // delete item and reinitialize
      goto begin; */
      if (!m_newFlows.empty ())
        {
	  m_oldFlows.push_back (m_newFlows.front ());
	  m_newFlows.pop_front ();
	}
      else
        {
	  // the queue must be removed from the list
	  m_ht[m_oldFlows.front ()->h] = 0; // remove reference from the m_ht
	  m_oldFlows.pop_front ();
	}
      goto begin;

    }
  NS_LOG_DEBUG ("fq_codel found a packet "<<flow->h);
      
  flow->deficit -= item->GetPacketSize();
  flow->backlog -= item->GetPacketSize();
  backlog -= item->GetPacketSize();

  return item; 
}

Ptr<const QueueDiscItem>
Fq_CoDelQueue::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  /* struct list_head *head;

  head = &m_new_flows;
  if (list_empty(head)) {
    head = &m_old_flows;
    if (list_empty(head))
      return 0;
  }
  return list_first_entry(head, Fq_CoDelSlot, flowchain)->q->Peek(); */

  Ptr<Fq_CoDelSlot> flow;
  if (!m_newFlows.empty ())
    {
      flow = m_newFlows.front ();
    }
  else if (!m_oldFlows.empty ())
    {
      flow = m_oldFlows.front ();
    }
  else
    {
      return 0; 
    }
  return flow->q->Peek ();
}

bool
Fq_CoDelQueue::CheckConfig (void)
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

  return true;
}

void
Fq_CoDelQueue::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}

} // namespace ns3
