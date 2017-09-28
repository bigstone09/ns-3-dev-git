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

  /**
   * Set the emulated device name of this device.
   * \param deviceName The emulated device name of this device.
   */
  void SetDeviceName (std::string deviceName);

  /**
   * Switch the interface in netmap mode.
   */
  bool NetmapOpen ();

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
   * This function write a packet into the netmap transmission ring.
   * \param buffer pointer to the packet to write
   * \param lenght lenght of the packet
   * \return the number of writed bytes
   */
  virtual ssize_t Write (uint8_t *buffer, size_t length);

  /**
   * Get the NetDeviceQueue associated with the netmap transmission ring.
   * \returns The NetDeviceQueue associated with the netmap transmission ring.
   */
  Ptr<NetDeviceQueue> GetTxQueue () const;

protected:

  /**
   * Spin up the device
   */
  void StartDevice (void);

  /**
   * Tear down the device
   */
  void StopDevice (void);

private:


  /**
   * This function read a packet from the netmap receiver ring.
   * \param buffer pointer to destination memory area
   * \return the number of read bytes
   */
  virtual ssize_t Read (uint8_t * buffer);

  /**
   * \brief This function waits for the next available slot in the netmap tx ring.
   * This function runs in a separate thread.
   */
  virtual void WaitingSlot ();

  std::string m_deviceName; //!< Name of the netmap emulated device

  struct netmap_if *m_nifp; //!< Netmap interface representation

  int m_nTxRings; //!< Number of transmission rings
  int m_nRxRings; //!< Number of receiver rings

  int m_nTxRingsSlots; //!< Number of slots in the transmission rings
  int m_nRxRingsSlots; //!< Number of slots in the receiver rings

  Ptr<NetDeviceQueueInterface> m_queueInterface; //!< NetDevice queue interface
  Ptr<NetDeviceQueue> m_queue;                   //!< NetDevice queue

  Ptr<SystemThread> m_waitingSlotThread; //!< Thread used to perform the flow control
  bool m_waitingSlotThreadRun;           //!< Running flag of the flow control thread
  SystemCondition m_queueStopped;        //!< Waiting condition of the flow control thread

  uint32_t m_totalQueuedBytes;           //!< Total queued bytes

};

} // namespace ns3

#endif /* NETMAP_NET_DEVICE_H */
