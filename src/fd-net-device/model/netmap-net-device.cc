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
#include "ns3/simulator.h"
#include "ns3/system-thread.h"
#include "ns3/system-condition.h"
#include "ns3/system-mutex.h"

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <poll.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NetmapNetDevice");

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
  m_deviceName = "undefined";
  m_nifp = 0;
  m_nTxRings = 0;
  m_nRxRings = 0;
  m_nTxRingsSlots = 0;
  m_nRxRingsSlots = 0;
  m_queue = 0;
  m_totalQueuedBytes = 0;
}

NetmapNetDevice::~NetmapNetDevice ()
{
  NS_LOG_FUNCTION (this);
  m_waitingSlotThreadRun = false;
}

void
NetmapNetDevice::SetDeviceName (std::string deviceName)
{
  NS_LOG_FUNCTION (this);

  m_deviceName = deviceName;
}

void
NetmapNetDevice::StartDevice (void)
{
  NS_LOG_FUNCTION (this);
  ns3::FdNetDevice::StartDevice ();

}

void
NetmapNetDevice::StopDevice (void)
{
  NS_LOG_FUNCTION (this);
  ns3::FdNetDevice::StopDevice ();

  m_waitingSlotThreadRun = false;
  m_queueStopped.SetCondition (true);
  m_queueStopped.Signal ();

}

bool
NetmapNetDevice::NetmapOpen ()
{
  NS_LOG_FUNCTION (this);

  if (m_deviceName == "undefined")
    {
      NS_FATAL_ERROR ("NetmapNetDevice: m_deviceName is not set");
    }

  if (m_fd == -1)
    {
      NS_FATAL_ERROR ("NetmapNetDevice: m_fd is not set");
    }

  struct nmreq nmr;
  memset (&nmr, 0, sizeof (nmr));

  nmr.nr_version = NETMAP_API;

  // setting the interface name in the netmap request
  strncpy (nmr.nr_name, m_deviceName.c_str (), m_deviceName.length ());

  // switch the interface in netmap mode
  int code = ioctl (m_fd, NIOCREGIF, &nmr);
  if (code == -1)
    {
      NS_LOG_DEBUG ("Switching failed");
      return false;
    }

  // memory mapping
  uint8_t *memory = (uint8_t *) mmap (0, nmr.nr_memsize, PROT_WRITE | PROT_READ,
                                      MAP_SHARED, m_fd, 0);

  if (memory == MAP_FAILED)
    {
      NS_LOG_DEBUG ("Mapping failed");
      return false;
    }

  // getting the base struct of the interface in netmap mode
  m_nifp = NETMAP_IF (memory, nmr.nr_offset);

  if (!m_nifp)
    {
      return false;
    }

  m_nTxRings = m_nifp->ni_tx_rings;
  m_nRxRings = m_nifp->ni_rx_rings;
  m_nTxRingsSlots = nmr.nr_tx_slots;
  m_nRxRingsSlots = nmr.nr_rx_slots;

  m_waitingSlotThreadRun = true;
  m_waitingSlotThread = Create<SystemThread> (MakeCallback (&NetmapNetDevice::WaitingSlot, this));
  m_waitingSlotThread->Start ();

  return true;

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

// runs in a separate thread
void
NetmapNetDevice::WaitingSlot ()
{
  NS_LOG_FUNCTION (this);

  struct pollfd fds;
  memset (&fds, 0, sizeof(fds));

  fds.fd = m_fd;
  fds.events = POLLOUT;

  struct netmap_ring *txring = NETMAP_TXRING (m_nifp, 0);

  uint32_t prevTotalTransmittedBytes = 0;

  while (m_waitingSlotThreadRun)
    {
      // we are waiting for the next queue stopped event
      // in meanwhile, we periodically sync the netmap ring and notify about the transmitted bytes in the period
      while (!m_queueStopped.GetCondition ())
        {
          // we sync the netmap ring periodically.
          // traffic control can write packets during the period between two syncs.
          ioctl (m_fd, NIOCTXSYNC, NULL);

          // we need of a nearly periodic notification to queue limits of the transmitted bytes.
          // we use this thread to notify about the transmitted bytes in the sleep period (alternatively, we can consider a
          // periodic schedule of a function).

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
            }

          // we used a period to check and notify of 200 us; it is a value close to the interrupt coalescence
          // period of a real device
          m_queueStopped.TimedWait (200000); // ns
        }
      m_queueStopped.SetCondition (false);

      // we are blocked for the next slot available in the netmap ring.
      // for netmap in emulated mode, if you disable the generic_txqdisc you are unblocked for the actual next slot available.
      // conversely, without disabling the generic_txqdisc you are unblocked when in the generic_txqdisc there are no packets.
      ioctl (m_fd, NIOCTXSYNC, NULL);

      poll (&fds, 1, -1);

      NS_LOG_DEBUG ("Space in the netmap ring of " << nm_ring_space (txring) << " packets");

      // we can now wake the queue.
      if (m_queue)
        {
          m_queue->Wake ();
        }
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
      // then, we wake up the thread to wait for the next slot available. in meanwhile, the main process can
      // run separately.
      if (nm_ring_space(txring) == 0)
        {
          m_queue->Stop ();

          m_queueStopped.SetCondition (true);
          m_queueStopped.Signal ();
        }
    }

  return ret;
}

// runs in a separate thread
ssize_t
NetmapNetDevice::Read (uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);

  struct netmap_ring *rxring;

  uint16_t lenght = 0;

  int rxRingIndex = 0;
  // we have a packet in one of the receiver rings.
  // we check for the first non empty receiver ring of a multiqueue device.
  while (rxRingIndex < m_nRxRings)
    {
      rxring = NETMAP_RXRING (m_nifp, rxRingIndex);

      if (!nm_ring_empty (rxring))
        {

          uint32_t i   = rxring->cur;
          uint8_t *buf = (uint8_t*) NETMAP_BUF (rxring, rxring->slot[i].buf_idx);
          lenght = rxring->slot[i].len;
          NS_LOG_DEBUG ("Received a packet of " << lenght << " bytes");

          // copy buffer in the destination memory area
          memcpy (buffer, buf, lenght);

          // advance the netmap pointers
          rxring->head = rxring->cur = nm_ring_next (rxring, i);

          ioctl (m_fd, NIOCRXSYNC, NULL);

          return lenght;

        }

      rxRingIndex++;
    }

  return lenght;

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

Ptr<NetDeviceQueue>
NetmapNetDevice::GetTxQueue () const
{
  NS_LOG_FUNCTION (this);
  return m_queue;
}

} // namespace ns3
