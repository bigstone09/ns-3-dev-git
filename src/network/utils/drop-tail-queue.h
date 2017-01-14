/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

#ifndef DROPTAIL_H
#define DROPTAIL_H

#include "ns3/queue.h"

namespace ns3 {

/**
 * \ingroup queue
 *
 * \brief A FIFO packet queue that drops tail-end packets on overflow
 */
template <typename Item>
class DropTailQueue : public Queue<Item>
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief DropTailQueue Constructor
   *
   * Creates a droptail queue with a maximum size of 100 packets by default
   */
  DropTailQueue ();

  virtual ~DropTailQueue ();

  virtual bool Enqueue (Ptr<Item> item);
  virtual Ptr<Item> Dequeue (void);
  virtual Ptr<Item> Remove (void);
  virtual Ptr<const Item> Peek (void) const;

private:
  using QueueBase::NsLog;
  using Queue<Item>::Head;
  using Queue<Item>::Tail;
  using Queue<Item>::DoEnqueue;
  using Queue<Item>::DoDequeue;
  using Queue<Item>::DoRemove;
  using Queue<Item>::DoPeek;
};


/**
 * Implementation of the templates declared above.
 */

template <typename Item>
TypeId
DropTailQueue<Item>::GetTypeId (void)
{
  static TypeId tid = TypeId (("ns3::DropTailQueue<" + GetTypeParamName<DropTailQueue<Item> > () + ">").c_str ())
    .SetParent<Queue<Item> > ()
    .SetGroupName ("Network")
    .template AddConstructor<DropTailQueue<Item> > ()
  ;
  return tid;
}

template <typename Item>
DropTailQueue<Item>::DropTailQueue () :
  Queue<Item> ()
{
  NsLog (LOG_DEBUG, "DropTailQueue ", this);
}

template <typename Item>
DropTailQueue<Item>::~DropTailQueue ()
{
  NsLog (LOG_DEBUG, "~DropTailQueue ", this);
}

template <typename Item>
bool
DropTailQueue<Item>::Enqueue (Ptr<Item> item)
{
  NsLog (LOG_DEBUG, "Enqueue ", this, item);

  return DoEnqueue (Tail (), item);
}

template <typename Item>
Ptr<Item>
DropTailQueue<Item>::Dequeue (void)
{
  NsLog (LOG_DEBUG, "Dequeue ", this);

  Ptr<Item> item = DoDequeue (Head ());

  NsLog (LOG_LOGIC, "Popped ", item);

  return item;
}

template <typename Item>
Ptr<Item>
DropTailQueue<Item>::Remove (void)
{
  NsLog (LOG_DEBUG, "Remove ", this);

  Ptr<Item> item = DoRemove (Head ());

  NsLog (LOG_LOGIC, "Removed ", item);

  return item;
}

template <typename Item>
Ptr<const Item>
DropTailQueue<Item>::Peek (void) const
{
  NsLog (LOG_DEBUG, "Peek ", this);

  return DoPeek (Head ());
}

} // namespace ns3

#endif /* DROPTAIL_H */
