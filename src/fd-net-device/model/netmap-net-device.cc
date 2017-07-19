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

#include "netmap-net-device.h"
#include "ns3/log.h"

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

  return true;

}

ssize_t
NetmapNetDevice::Write (uint8_t* buffer, size_t length)
{
  NS_LOG_FUNCTION (this << buffer << length);

  return 0;
}

// runs in a separate thread
ssize_t
NetmapNetDevice::Read (uint8_t* buffer)
{
  NS_LOG_FUNCTION (this << buffer);

  return 0;
}

} // namespace ns3
