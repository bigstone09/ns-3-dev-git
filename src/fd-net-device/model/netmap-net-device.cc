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
  m_txNotificationPeriod = Time (MicroSeconds (500));
}

NetmapNetDevice::~NetmapNetDevice ()
{
  NS_LOG_FUNCTION (this);
}

void
NetmapNetDevice::SetDeviceName (std::string deviceName)
{
  NS_LOG_FUNCTION (this);

  m_deviceName = deviceName;
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
  // setting of netmap flags, for instance
  // nmr.nr_flags |= NR_TX_RINGS_ONLY;

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

  return true;

}

void
NetmapNetDevice::WaitingSlot ()
{
  NS_LOG_FUNCTION (this);

  struct pollfd fds;

  fds.fd = m_fd;
  fds.events = POLLOUT;

  struct netmap_ring *txring = NETMAP_TXRING (m_nifp, 0);

  // if the queue was stopped we wait for the next available slot on the netmap ring and call the wake
  if (m_queueInterface->GetTxQueue (0)->IsStopped ())
    {
      // we blocked for the next slot available in the netmap ring
      // for netmap in emulated mode, if you disable the generic_txqdisc you are unblocked for the actual next slot available.
      // conversely, without disabling the generic_txqdisc you are unblocked when in the generic_txqdisc there are no packets
      poll (&fds, 1, -1);

      NS_LOG_DEBUG ("Space in the netmap ring of " << nm_ring_space (txring) << " packets");

      m_queueInterface->GetTxQueue (0)->Wake ();
    }
}

ssize_t
NetmapNetDevice::Write (uint8_t* buffer, size_t length)
{
  NS_LOG_FUNCTION (this << buffer << length);

  struct netmap_ring *txring;

  txring = NETMAP_TXRING (m_nifp, 0);

  uint16_t ret = -1;

  if (m_queueInterface->GetTxQueue (0)->IsStopped ())
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

      ioctl (m_fd, NIOCTXSYNC, NULL);

      ret = length;

      // if there is no room for other packets then stop the queue
      if (nm_ring_space(txring) == 0)
        {
          m_queueInterface->GetTxQueue (0)->Stop ();
         // schedule the interrupt notification to signal the transmission completed
         if (Simulator::IsExpired (m_id))
           {
             NS_LOG_DEBUG ("InterruptNotification schedule");
             m_id = Simulator::Schedule (m_txNotificationPeriod, &NetmapNetDevice::WaitingSlot, this);
           }
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

  rxring = NETMAP_RXRING (m_nifp, 0); // rx ring with index 0
                                      // we should check each ring of a multiqueue device?

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
}

} // namespace ns3
