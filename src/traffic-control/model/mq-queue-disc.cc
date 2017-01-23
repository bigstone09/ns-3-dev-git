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

#include "ns3/log.h"
#include "ns3/string.h"
#include "mq-queue-disc.h"
#include "pfifo-fast-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MqQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (MqQueueDisc);

TypeId MqQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MqQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<MqQueueDisc> ()
  ;
  return tid;
}

MqQueueDisc::MqQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

MqQueueDisc::~MqQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

bool
MqQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  return true;
}

Ptr<QueueDiscItem>
MqQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  return item;
}

Ptr<const QueueDiscItem>
MqQueueDisc::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  return item;
}

bool
MqQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (m_devQueueIface->GetNTxQueues () == 1)
    {
      NS_LOG_ERROR ("MqQueueDisc cannot be used on not multiqueue device");
      return false;
    }

  if (GetNQueueDiscClasses() != m_devQueueIface->GetNTxQueues ())
    {
      NS_LOG_ERROR ("MqQueueDisc cannot have a number of classes different from the number of device queues");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("MqQueueDisc cannot have packet filters");
      return false;
    }

  if (GetNInternalQueues () > 0)
    {
      NS_LOG_ERROR ("MqQueueDisc cannot have internal queues");
      return false;
    }

  return true;
}

void
MqQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);

  m_wakeMode = WAKE_CHILD;

}

} // namespace ns3
