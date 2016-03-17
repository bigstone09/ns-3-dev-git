/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/uinteger.h"
#include "net-device.h"
#include "packet.h"
#include "dynamic-queue-limits.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NetDevice");

QueueItem::QueueItem (Ptr<Packet> p)
{
  m_packet = p;
}

QueueItem::~QueueItem()
{
  NS_LOG_FUNCTION (this);
  m_packet = 0;
}

Ptr<Packet>
QueueItem::GetPacket (void) const
{
  return m_packet;
}

uint32_t
QueueItem::GetPacketSize (void) const
{
  NS_ASSERT (m_packet != 0);
  return m_packet->GetSize ();
}

void
QueueItem::Print (std::ostream& os) const
{
  os << GetPacket();
}

std::ostream & operator << (std::ostream &os, const QueueItem &item)
{
  item.Print (os);
  return os;
}

NetDeviceQueue::NetDeviceQueue()
  : m_stopped (false)
{
  NS_LOG_FUNCTION (this);

  m_queueLimits = CreateObject<DynamicQueueLimits> ();
  m_queueLimits->Init ();
  m_stoppedQueueLimits = false;
}

NetDeviceQueue::~NetDeviceQueue ()
{
  NS_LOG_FUNCTION (this);
}

bool
NetDeviceQueue::IsStopped (void) const
{
  return m_stopped || m_stoppedQueueLimits;
}

void
NetDeviceQueue::Start (void)
{
  m_stopped = false;
}

void
NetDeviceQueue::Stop (void)
{
  m_stopped = true;
}

void
NetDeviceQueue::Wake (void)
{
  Start ();

  // Request the queue disc to dequeue a packet
  if (!m_wakeCallback.IsNull ())
  {
      m_wakeCallback ();
  }
}

void
NetDeviceQueue::SetWakeCallback (WakeCallback cb)
{
  m_wakeCallback = cb;
}

bool
NetDeviceQueue::HasWakeCallbackSet (void) const
{
  return (!m_wakeCallback.IsNull ());
}

void
NetDeviceQueue::netdev_tx_sent_queue(uint32_t bytes)
{
  /* dql_queued(&dev_queue->dql, bytes);

  if (likely(dql_avail(&dev_queue->dql) >= 0))
	  return;

  set_bit(__QUEUE_STATE_STACK_XOFF, &dev_queue->state);

  *
    * The XOFF flag must be set before checking the dql_avail below,
    * because in netdev_tx_completed_queue we update the dql_completed
    * before checking the XOFF flag.
    *
  smp_mb();

  * check again in case another CPU has just made room avail *
  if (unlikely(dql_avail(&dev_queue->dql) >= 0))
	  clear_bit(__QUEUE_STATE_STACK_XOFF, &dev_queue->state); */

  m_queueLimits->Queued (bytes);
  if (m_queueLimits->Avail() >= 0) {
	  return;
  }
  m_stoppedQueueLimits = true;
}

void
NetDeviceQueue::netdev_tx_completed_queue(uint32_t pkts, uint32_t bytes)
{
  /* if (unlikely(!bytes))
	  return;

  dql_completed(&dev_queue->dql, bytes);

  *
    * Without the memory barrier there is a small possiblity that
    * netdev_tx_sent_queue will miss the update and cause the queue to
    * be stopped forever
    *
  smp_mb();

  if (dql_avail(&dev_queue->dql) < 0)
	  return;

  if (test_and_clear_bit(__QUEUE_STATE_STACK_XOFF, &dev_queue->state))
	  netif_schedule_queue(dev_queue); */

  if (!bytes){
    return;
  }
  m_queueLimits->Completed (bytes);
  if (m_queueLimits->Avail () < 0){
    return;
  }
  if (m_stoppedQueueLimits) {
	  m_stoppedQueueLimits = false;
	  ///\todo netif_schedule_queue(dev_queue);
  }
}

void
NetDeviceQueue::netdev_tx_reset_queue()
{
  m_queueLimits->Reset ();
}


NS_OBJECT_ENSURE_REGISTERED (NetDeviceQueueInterface);

TypeId NetDeviceQueueInterface::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NetDeviceQueueInterface")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NetDeviceQueueInterface::NetDeviceQueueInterface ()
  : m_queueDiscInstalled (false)
{
  NS_LOG_FUNCTION (this);
  Ptr<NetDeviceQueue> devQueue = Create<NetDeviceQueue> ();
  m_txQueuesVector.push_back (devQueue);
}

NetDeviceQueueInterface::~NetDeviceQueueInterface ()
{
  NS_LOG_FUNCTION (this);
}

Ptr<NetDeviceQueue>
NetDeviceQueueInterface::GetTxQueue (uint8_t i) const
{
  NS_ASSERT (i < m_txQueuesVector.size ());
  return m_txQueuesVector[i];
}

uint8_t
NetDeviceQueueInterface::GetTxQueuesN (void) const
{
  return m_txQueuesVector.size ();
}

void
NetDeviceQueueInterface::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_txQueuesVector.clear ();
  Object::DoDispose ();
}

void
NetDeviceQueueInterface::SetTxQueuesN (uint8_t numTxQueues)
{
  NS_ASSERT (numTxQueues > 0);

  // check whether a queue disc has been installed on the device by
  // verifying whether a wake callback has been set on a transmission queue
  NS_ABORT_MSG_IF (GetTxQueue (0)->HasWakeCallbackSet (), "Cannot change the number of"
                   " transmission queues after setting up the wake callback.");

  uint8_t prevNumTxQueues = m_txQueuesVector.size ();
  m_txQueuesVector.resize (numTxQueues);

  // Allocate new NetDeviceQueues if the number of queues increased
  for (uint8_t i = prevNumTxQueues; i < numTxQueues; i++)
    {
      Ptr<NetDeviceQueue> devQueue = Create<NetDeviceQueue> ();
      m_txQueuesVector[i] = devQueue;
    }
}

void
NetDeviceQueueInterface::SetSelectQueueCallback (SelectQueueCallback cb)
{
  m_selectQueueCallback = cb;
}

uint8_t
NetDeviceQueueInterface::GetSelectedQueue (Ptr<QueueItem> item) const
{
  if (!m_selectQueueCallback.IsNull ())
  {
    return m_selectQueueCallback (item);
  }
  return 0;
}

bool
NetDeviceQueueInterface::IsQueueDiscInstalled (void) const
{
  return m_queueDiscInstalled;
}

void
NetDeviceQueueInterface::SetQueueDiscInstalled (bool installed)
{
  NS_LOG_FUNCTION (this << installed);
  m_queueDiscInstalled = installed;
}


NS_OBJECT_ENSURE_REGISTERED (NetDevice);

TypeId NetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NetDevice")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NetDevice::~NetDevice ()
{
  NS_LOG_FUNCTION (this);
}

} // namespace ns3
