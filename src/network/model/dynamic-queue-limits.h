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

#ifndef DYNAMIC_QUEUE_LIMITS_H
#define DYNAMIC_QUEUE_LIMITS_H

#include "queue-limits.h"
#include "ns3/nstime.h"
#include <limits.h>

namespace ns3 {

/* Set some static maximums */
#define DQL_MAX_OBJECT (UINT_MAX / 16)
#define DQL_MAX_LIMIT ((UINT_MAX / 2) - DQL_MAX_OBJECT)

class DynamicQueueLimits : public QueueLimits {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  DynamicQueueLimits ();
  virtual ~DynamicQueueLimits ();

  /* Initialize dql state */
  virtual int32_t Init ();

  /* Reset dql state */
  virtual void Reset ();

  /* Record number of completed objects and recalculate the limit. */
  virtual void Completed (uint32_t count);

  /* Returns how many objects can be queued, < 0 indicates over limit. */
  /* static */ virtual int32_t Avail () const;

  /*
  * Record number of objects queued. Assumes that caller has already checked
  * availability in the queue with dql_avail.
  */
  /* static */ virtual void Queued (uint32_t count);

private:
  struct dql 
  {
    /* Fields accessed in enqueue path (dql_queued) */
    uint32_t num_queued;	/* Total ever queued */
    uint32_t adj_limit;		/* limit + num_completed */
    uint32_t last_obj_cnt;	/* Count at last queuing */

    /* Fields accessed only by completion path (dql_completed) */

    uint32_t limit; // ____cacheline_aligned_in_smp; /* Current limit */
    uint32_t num_completed;	/* Total ever completed */

    uint32_t prev_ovlimit;	/* Previous over limit */
    uint32_t prev_num_queued;	/* Previous queue total */
    uint32_t prev_last_obj_cnt;	/* Previous queuing cnt */

    uint32_t lowest_slack;	/* Lowest slack found */
    int64_t slack_start_time;	/* Time slacks seen */

    /* Configuration */
    uint32_t max_limit;		/* Max limit */
    uint32_t min_limit;		/* Minimum limit */
    int64_t slack_hold_time;	/* Time to measure slack */
  };

  struct dql m_dql;
  Time m_holdTime;
};

} // namespace ns3

#endif /* DYNAMIC_QUEUE_LIMITS_H */
