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
 */

#ifndef NETMAP_NET_DEVICE_H
#define NETMAP_NET_DEVICE_H

#include "fd-net-device.h"
#include <net/netmap_user.h>

namespace ns3 {

class Node;

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
   *
   * \param deviceName The emulated device name of this device.
   */
  void SetDeviceName (std::string deviceName);

  bool NetmapOpen ();

protected:

private:

  virtual ssize_t Write (uint8_t *buffer, size_t length);
  
  virtual ssize_t Read (uint8_t * buffer);

  std::string m_deviceName; 

  struct netmap_if *m_nifp; //<<! Netmap interface representation

};

} // namespace ns3

#endif /* NETMAP_NET_DEVICE_H */
