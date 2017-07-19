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

#include "netmap-net-device.h"
#include "ns3/log.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/system-thread.h"

#include <sys/ioctl.h>

#include <unistd.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NetmapNetDevice");

NetmapNetDeviceFdReader::NetmapNetDeviceFdReader ()
  : m_bufferSize (65536), // Defaults to maximum TCP window size
    m_nifp (0)
{
}

void
NetmapNetDeviceFdReader::SetBufferSize (uint32_t bufferSize)
{
  NS_LOG_FUNCTION (this << bufferSize);
  m_bufferSize = bufferSize;
}

void
NetmapNetDeviceFdReader::SetNetmapIfp (struct netmap_if *nifp)
{
  NS_LOG_FUNCTION (this << nifp);
  m_nifp = nifp;
}

FdReader::Data
NetmapNetDeviceFdReader::DoRead (void)
{
  NS_LOG_FUNCTION (this);

  uint8_t *buf = (uint8_t *)malloc (m_bufferSize);
  NS_ABORT_MSG_IF (buf == 0, "malloc() failed");

  NS_LOG_LOGIC ("Calling read on fd " << m_fd);

  struct netmap_ring *rxring;
  uint16_t len = 0;
  uint32_t rxRingIndex = 0;

  // we have a packet in one of the receiver rings.
  // we check for the first non empty receiver ring of a multiqueue device.
  while (rxRingIndex < m_nifp->ni_rx_rings)
    {
      rxring = NETMAP_RXRING (m_nifp, rxRingIndex);

      if (!nm_ring_empty (rxring))
        {
          uint32_t i = rxring->cur;
          uint8_t *buffer = (uint8_t*) NETMAP_BUF (rxring, rxring->slot[i].buf_idx);
          len = rxring->slot[i].len;
          NS_LOG_DEBUG ("Received a packet of " << len << " bytes");

          // copy buffer in the destination memory area
          memcpy (buf, buffer, len);

          // advance the netmap pointers
          rxring->head = rxring->cur = nm_ring_next (rxring, i);

          ioctl (m_fd, NIOCRXSYNC, NULL);

          break;
        }

      rxRingIndex++;
    }

  if (len <= 0)
    {
      free (buf);
      buf = 0;
      len = 0;
    }
  NS_LOG_LOGIC ("Read " << len << " bytes on fd " << m_fd);
  return FdReader::Data (buf, len);
}

NS_OBJECT_ENSURE_REGISTERED (NetmapNetDevice);

TypeId
NetmapNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NetmapNetDevice")
    .SetParent<FdNetDevice> ()
    .SetGroupName ("FdNetDevice")
    .AddConstructor<NetmapNetDevice> ()
  ;
  return tid;
}

NetmapNetDevice::NetmapNetDevice ()
{
  NS_LOG_FUNCTION (this);
  m_nifp = 0;
  m_nTxRingsSlots = 0;
  m_queue = 0;
  m_totalQueuedBytes = 0;
}

NetmapNetDevice::~NetmapNetDevice ()
{
  NS_LOG_FUNCTION (this);
  m_waitingSlotThreadRun = false;
}

Ptr<FdReader>
NetmapNetDevice::DoCreateFdReader (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<NetmapNetDeviceFdReader> fdReader = Create<NetmapNetDeviceFdReader> ();
  // 22 bytes covers 14 bytes Ethernet header with possible 8 bytes LLC/SNAP
  fdReader->SetBufferSize (GetMtu () + 22);
  fdReader->SetNetmapIfp (m_nifp);
  return fdReader;
}

void
NetmapNetDevice::DoFinishStoppingDevice (void)
{
  NS_LOG_FUNCTION (this);

  m_waitingSlotThreadRun = false;
}

uint32_t
NetmapNetDevice::GetBytesInNetmapTxRing ()
{
  NS_LOG_FUNCTION (this);

  struct netmap_ring *txring;
  txring = NETMAP_TXRING (m_nifp, 0);

  int tail = txring->tail;

  // the netmap ring has one slot reserved
  int inQueue = (m_nTxRingsSlots -1) - nm_ring_space (txring);

  uint32_t bytesInQueue = 0;

  for (int i = 1; i < inQueue; i++)
    {
      bytesInQueue += txring->slot[tail].len;
      tail ++;
      tail = tail % m_nTxRingsSlots;
    }

  return bytesInQueue;
}

int
NetmapNetDevice::GetSpaceInNetmapTxRing () const
{
  NS_LOG_FUNCTION (this);

  struct netmap_ring *txring;
  txring = NETMAP_TXRING (m_nifp, 0);

  return nm_ring_space (txring);
}

void
NetmapNetDevice::StartWaitingSlot (struct netmap_if *nifp, uint32_t nTxRingsSlots, uint32_t nRxRingsSlots)
{
  NS_LOG_FUNCTION (this << nifp);

  m_nifp = nifp;
  m_nTxRings = m_nifp->ni_tx_rings;
  m_nRxRings = m_nifp->ni_rx_rings;
  m_nTxRingsSlots = nTxRingsSlots;
  m_nRxRingsSlots = nRxRingsSlots;

  m_waitingSlotThreadRun = true;
  m_waitingSlotThread = Create<SystemThread> (MakeCallback (&NetmapNetDevice::WaitingSlot, this));
  m_waitingSlotThread->Start ();
}

// runs in a separate thread
void
NetmapNetDevice::WaitingSlot ()
{
  NS_LOG_FUNCTION (this);

  struct netmap_ring *txring = NETMAP_TXRING (m_nifp, 0);

  uint32_t prevTotalTransmittedBytes = 0;

  while (m_waitingSlotThreadRun)
    {
      // we sync the netmap ring periodically.
      // the traffic control layer can write packets during the period between two syncs.
      ioctl (GetFileDescriptor (), NIOCTXSYNC, NULL);

      // we need of a nearly periodic notification to queue limits of the transmitted bytes.
      // we calculate the total transmitted bytes as differences between the total queued and the current in queue
      // also, we calculate the transmitted bytes in the sleep period as difference between the current total transmitted
      // and the previous total transmitted
      uint32_t totalTransmittedBytes = m_totalQueuedBytes - GetBytesInNetmapTxRing ();
      uint32_t deltaBytes = totalTransmittedBytes - prevTotalTransmittedBytes;
      NS_LOG_DEBUG (deltaBytes << " delta transmitted bytes");
      prevTotalTransmittedBytes = totalTransmittedBytes;
      if (m_queue)
        {
          m_queue->NotifyTransmittedBytes (deltaBytes);

          if (GetSpaceInNetmapTxRing () >= 32) // WAKE_THRESHOLD
            {
              if (m_queue->IsStopped ())
                {
                  m_queue->Wake ();
                }
            }
        }

      // we use a period to sync, check and notify of 200 us; it is a value close to the interrupt coalescence
      // period of a real device
      usleep (200);

      NS_LOG_DEBUG ("Space in the netmap ring of " << nm_ring_space (txring) << " packets");
   }
}

ssize_t
NetmapNetDevice::Write (uint8_t* buffer, size_t length)
{
  NS_LOG_FUNCTION (this << buffer << length);

  struct netmap_ring *txring;

  // we use one ring also in case of multiqueue device to perform a fine flow control on that ring
  txring = NETMAP_TXRING (m_nifp, 0);

  uint16_t ret = -1;

  if (m_queue->IsStopped ())
    {
      // the device queue is stopped and we cannot write other packets
      return ret;
    }

  if (!nm_ring_empty (txring))
    {

      uint32_t i = txring->cur;
      uint8_t* buf = (uint8_t*) NETMAP_BUF (txring, txring->slot[i].buf_idx);

      memcpy (buf, buffer, length);
      txring->slot[i].len = length;

      txring->head = txring->cur = nm_ring_next (txring, i);

      ret = length;

      // we update the total transmitted bytes counter and notify queue limits of the queued bytes
      m_totalQueuedBytes += length;
      m_queue->NotifyQueuedBytes (length);

      // if there is no room for other packets then stop the queue.
      // then, we wake up the thread to wait for the next slot available. In the
      // meanwhile, the main process can run separately.
      if (nm_ring_space(txring) == 0)
        {
          m_queue->Stop ();
        }
    }

  return ret;
}

void
NetmapNetDevice::NotifyNewAggregate (void)
{
  NS_LOG_FUNCTION (this);
  if (m_queueInterface == 0)
    {
      Ptr<NetDeviceQueueInterface> ndqi = this->GetObject<NetDeviceQueueInterface> ();
      //verify that it's a valid netdevice queue interface and that
      //the netdevice queue interface was not set before
      if (ndqi != 0)
        {
          m_queueInterface = ndqi;
        }
    }
  NetDevice::NotifyNewAggregate ();
  m_queueInterface->SetTxQueuesN (1);
  m_queueInterface->CreateTxQueues ();
  m_queue = m_queueInterface->GetTxQueue (0);
}

} // namespace ns3
