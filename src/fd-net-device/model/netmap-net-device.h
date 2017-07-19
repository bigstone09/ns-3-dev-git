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

#ifndef NETMAP_NET_DEVICE_H
#define NETMAP_NET_DEVICE_H

#include "fd-net-device.h"
#include <net/netmap_user.h>

namespace ns3 {

/**
 * \ingroup fd-net-device
 * \brief This class performs the actual data reading from the netmap ring.
 */
class NetmapNetDeviceFdReader : public FdReader
{
public:
  NetmapNetDeviceFdReader ();

  /**
   * \brief Set size of the read buffer.
   * \param bufferSize the size of the read buffer
   */
  void SetBufferSize (uint32_t bufferSize);

  /**
   * \brief Set size netmap interface representation.
   * \param nifp the netmap interface representation
   */
  void SetNetmapIfp (struct netmap_if *nifp);

private:
  FdReader::Data DoRead (void);

  uint32_t m_bufferSize;    //!< size of the read buffer
  struct netmap_if *m_nifp; //!< Netmap interface representation
};

class Node;
class NetDeviceQueueInterface;
class SystemThread;
class NetDeviceQueue;

/**
 * \ingroup fd-net-device
 *
 * \brief a NetDevice to read/write network traffic from/into a netmap file descriptor.
 *
 * A NetmapNetDevice object will read and write frames/packets from/to a netmap file descriptor.
 *
 */
class NetmapNetDevice : public FdNetDevice
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * Constructor for the NetmapNetDevice.
   */
  NetmapNetDevice ();

  /**
   * Destructor for the NetmapNetDevice.
   */
  virtual ~NetmapNetDevice ();

  virtual void NotifyNewAggregate (void);

  /**
   * Get the number of bytes currently in the netmap transmission ring.
   * \returns The number of bytes in the netmap transmission ring.
   */
  uint32_t GetBytesInNetmapTxRing ();

  /**
   * Get the number of slots currently available in the netmap transmission ring.
   * \returns The number of slots currently available in the netmap transmission ring.
   */
  int GetSpaceInNetmapTxRing () const;

  /**
   * This function writes a packet into the netmap transmission ring.
   */
  virtual ssize_t Write (uint8_t *buffer, size_t length);

  /**
   * Start the thread waiting for an available slot.
   *
   * \param nifp the netmap interface representation
   * \param nTxRingsSlots the number of slots in the transmission rings
   * \param nRxRingsSlots the number of slots in the receiver rings
   */
  void StartWaitingSlot (struct netmap_if *nifp, uint32_t nTxRingsSlots, uint32_t nRxRingsSlots);

private:
  Ptr<FdReader> DoCreateFdReader (void);
  void DoFinishStoppingDevice (void);

  /**
   * \brief This function waits for the next available slot in the netmap tx ring.
   * This function runs in a separate thread.
   */
  virtual void WaitingSlot ();

  struct netmap_if *m_nifp; //!< Netmap interface representation

  uint32_t m_nTxRings; //!< Number of transmission rings
  uint32_t m_nRxRings; //!< Number of receiver rings

  uint32_t m_nTxRingsSlots; //!< Number of slots in the transmission rings
  uint32_t m_nRxRingsSlots; //!< Number of slots in the receiver rings

  Ptr<NetDeviceQueueInterface> m_queueInterface; //!< NetDevice queue interface
  Ptr<NetDeviceQueue> m_queue;                   //!< NetDevice queue

  Ptr<SystemThread> m_waitingSlotThread; //!< Thread used to perform the flow control
  bool m_waitingSlotThreadRun;           //!< Running flag of the flow control thread

  uint32_t m_totalQueuedBytes;           //!< Total queued bytes
};

} // namespace ns3

#endif /* NETMAP_NET_DEVICE_H */
