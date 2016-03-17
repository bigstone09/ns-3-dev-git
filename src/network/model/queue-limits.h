/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 Universita' degli Studi di Napoli Federico II
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

#ifndef QUEUE_LIMITS_H
#define QUEUE_LIMITS_H

#include "ns3/object.h"

namespace ns3 {

class QueueLimits : public Object {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  QueueLimits ();
  virtual ~QueueLimits ();

  /* Initialize dql state */
  virtual int32_t Init () = 0;

  /* Reset dql state */
  virtual void Reset () = 0;

  /* Record number of completed objects and recalculate the limit. */
  virtual void Completed (uint32_t count) = 0;

  /* Returns how many objects can be queued, < 0 indicates over limit. */
  /* static */ virtual int32_t Avail () const = 0;

  /*
  * Record number of objects queued. Assumes that caller has already checked
  * availability in the queue with dql_avail.
  */
  /* static */ virtual void Queued (uint32_t count) = 0;
};

} // namespace ns3

#endif /* QUEUE_LIMITS_H */
