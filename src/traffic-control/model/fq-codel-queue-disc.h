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
 * Authors: Pasquale Imputato <p.imputato@gmail.com>
 *          Stefano Avallone <stefano.avallone@unina.it>
 */

#ifndef FQ_CODEL_QUEUE_DISC
#define FQ_CODEL_QUEUE_DISC

#include "ns3/queue-disc.h"
#include "ns3/object-factory.h"
#include <list>
#include <map>

namespace ns3 {

class FQCoDelFlow : public QueueDiscClass {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief FQCoDelFlow constructor
   */
  FQCoDelFlow ();

  virtual ~FQCoDelFlow ();

  /**
   * \brief Set the credits for this flow
   * \param credits the credits for this flow
   */
  void SetCredits (int32_t credits);
  /**
   * \brief Get the credits for this flow
   * \return the credits for this flow
   */
  int32_t GetCredits ();
  /**
   * \brief Increase the credits for this flow
   * \param credits the credits to add
   */
  void AddCredits (int32_t credits);
  /**
   * \brief Decrease the credits for this flow
   * \param credits the credits to subtract
   */
  void SubCredits (int32_t credits);
  /**
   * \brief Set the active flag for this flow
   * \param active the active flag for this flow
   */
  void SetActive (bool active);
  /**
   * \brief Get the active flag for this flow
   * \return the active flag for this flow
   */
  bool GetActive ();

private:
  int32_t m_credits;  //!< the credits for this flow
  bool m_active;      //!< the active flag for this flow
};

class FQCoDelQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief FQCoDelQueueDisc constructor
   */
  FQCoDelQueueDisc ();

  virtual ~FQCoDelQueueDisc ();

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void) const;
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);

  /**
   * \brief Drop a packet from the head of the queue with the largest current byte count
   * \return the index of the queue with the largest current byte count
   */
  uint32_t FQCoDelDrop ();

  std::string m_interval;  //!< CoDel interval attribute
  std::string m_target;    //!< CoDel target attribute
  uint32_t m_limit;        //!< Maximum number of packets in the queue disc
  uint32_t m_quantum;      //!< Credits granted to flows at each round
  uint32_t m_flows;        //!< Number of flow queues

  uint32_t m_overlimitDroppedPackets; //!< Number of overlimit dropped packets

  std::list<Ptr<FQCoDelFlow> > m_newFlows;    //!< The list of new flows
  std::list<Ptr<FQCoDelFlow> > m_oldFlows;    //!< The list of old flows

  std::map<uint32_t, uint32_t> m_flowsIndices;    //!< Map with the index of class for each flow

  ObjectFactory m_flowFactory;         //!< Factory to create a new flow
  ObjectFactory m_queueDiscFactory;    //!< Factory to create a new queue
};

} // namespace ns3

#endif /* FQ_CODEL_QUEUE_DISC */
