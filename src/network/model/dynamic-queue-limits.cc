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

#include "ns3/log.h"
#include "ns3/integer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "dynamic-queue-limits.h"

namespace ns3 {

#define POSDIFF(A, B) ((int)((A) - (B)) > 0 ? (A) - (B) : 0)
#define AFTER_EQ(A, B) ((int)((A) - (B)) >= 0)

NS_LOG_COMPONENT_DEFINE ("DynamicQueueLimits");

NS_OBJECT_ENSURE_REGISTERED (DynamicQueueLimits);

TypeId
DynamicQueueLimits::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DynamicQueueLimits")
    .SetParent<Object> ()
    .AddAttribute ("HoldTime",
                   "The DQL algorithm hold time",
                   StringValue ("1ms"), // HZ
                   MakeTimeAccessor (&DynamicQueueLimits::m_holdTime),
                   MakeTimeChecker ())
  ;
  return tid;
}

DynamicQueueLimits::DynamicQueueLimits ()
{
  NS_LOG_FUNCTION (this);
}

DynamicQueueLimits::~DynamicQueueLimits ()
{
  NS_LOG_FUNCTION (this);
}

int32_t
DynamicQueueLimits::Init ()
{
  m_dql.max_limit = DQL_MAX_LIMIT;
  m_dql.min_limit = 0;
  m_dql.slack_hold_time = m_holdTime.GetInteger (); // hold_time;
  Reset ();
  return 0;
}

void
DynamicQueueLimits::Reset ()
{
  /* Reset all dynamic values */
  m_dql.limit = 0;
  m_dql.num_queued = 0;
  m_dql.num_completed = 0;
  m_dql.last_obj_cnt = 0;
  m_dql.prev_num_queued = 0;
  m_dql.prev_last_obj_cnt = 0;
  m_dql.prev_ovlimit = 0;
  m_dql.lowest_slack = UINT_MAX;
  m_dql.slack_start_time = Simulator::Now ().GetInteger (); // jiffies;
}

/*
*      These inlines deal with timer wrapping correctly. You are 
*      strongly encouraged to use them
*      1. Because people otherwise forget
*      2. Because if the timer wrap changes in future you won't have to
*         alter your driver code.
*
* time_after(a,b) returns true if the time a is after time b.
*
* Do this with "<0" and ">=0" to only test the sign of the result. A
* good compiler would generate better code (and a really good compiler
* wouldn't care). Gcc is currently neither.
*/
/* #define time_after(a,b)         \
        (typecheck(unsigned long, a) && \
         typecheck(unsigned long, b) && \
         ((long)((b) - (a)) < 0)) */

/**
* clamp - return a value clamped to a given range with strict typechecking
* @val: current value
* @lo: lowest allowable value
* @hi: highest allowable value
*
* This macro does strict typechecking of lo/hi to make sure they are of the
* same type as val.  See the unnecessary pointer comparisons.
*/
/* #define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi) */

void
DynamicQueueLimits::Completed (uint32_t count)
{
  uint32_t inprogress, prev_inprogress, limit;
  uint32_t ovlimit, completed, num_queued;
  bool all_prev_completed;

  /* num_queued = ACCESS_ONCE(dql->num_queued); */
  num_queued = m_dql.num_queued;

  /* Can't complete more than what's in queue */
  /* BUG_ON(count > num_queued - dql->num_completed); */
  NS_ASSERT (count <= num_queued - m_dql.num_completed);

  completed = m_dql.num_completed + count;
  limit = m_dql.limit;
  ovlimit = POSDIFF(num_queued - m_dql.num_completed, limit);
  inprogress = num_queued - completed;
  prev_inprogress = m_dql.prev_num_queued - m_dql.num_completed;
  all_prev_completed = AFTER_EQ(completed, m_dql.prev_num_queued);

  if ((ovlimit && !inprogress) ||
      (m_dql.prev_ovlimit && all_prev_completed)) {
	  /*
	    * Queue considered starved if:
	    *   - The queue was over-limit in the last interval,
	    *     and there is no more data in the queue.
	    *  OR
	    *   - The queue was over-limit in the previous interval and
	    *     when enqueuing it was possible that all queued data
	    *     had been consumed.  This covers the case when queue
	    *     may have becomes starved between completion processing
	    *     running and next time enqueue was scheduled.
	    *
	    *     When queue is starved increase the limit by the amount
	    *     of bytes both sent and completed in the last interval,
	    *     plus any previous over-limit.
	    */
	  limit += POSDIFF(completed, m_dql.prev_num_queued) +
		m_dql.prev_ovlimit;
	  m_dql.slack_start_time = Simulator::Now ().GetInteger (); // jiffies;
	  m_dql.lowest_slack = UINT_MAX;
  } else if (inprogress && prev_inprogress && !all_prev_completed) {
	  /*
	    * Queue was not starved, check if the limit can be decreased.
	    * A decrease is only considered if the queue has been busy in
	    * the whole interval (the check above).
	    *
	    * If there is slack, the amount of execess data queued above
	    * the the amount needed to prevent starvation, the queue limit
	    * can be decreased.  To avoid hysteresis we consider the
	    * minimum amount of slack found over several iterations of the
	    * completion routine.
	    */
	  uint32_t slack, slack_last_objs;

	  /*
	    * Slack is the maximum of
	    *   - The queue limit plus previous over-limit minus twice
	    *     the number of objects completed.  Note that two times
	    *     number of completed bytes is a basis for an upper bound
	    *     of the limit.
	    *   - Portion of objects in the last queuing operation that
	    *     was not part of non-zero previous over-limit.  That is
	    *     "round down" by non-overlimit portion of the last
	    *     queueing operation.
	    */
	  slack = POSDIFF(limit + m_dql.prev_ovlimit,
	      2 * (completed - m_dql.num_completed));
	  slack_last_objs = m_dql.prev_ovlimit ?
	      POSDIFF(m_dql.prev_last_obj_cnt, m_dql.prev_ovlimit) : 0;

	  slack = std::max (slack, slack_last_objs);

	  if (slack < m_dql.lowest_slack)
		  m_dql.lowest_slack = slack;

	  /* if (time_after(jiffies,
			  dql->slack_start_time + dql->slack_hold_time)) { */
	  /* if ((int64_t)((m_dql.slack_start_time + m_dql.slack_hold_time) - (Simulator::Now ().GetInteger ())) < 0) { */
	  if (((m_dql.slack_start_time + m_dql.slack_hold_time) - (Simulator::Now ().GetInteger ())) < 0) {
		  limit = POSDIFF(limit, m_dql.lowest_slack);
// 		  std::cout << "Limit " << limit << std::endl;
		  m_dql.slack_start_time = Simulator::Now ().GetInteger (); // jiffies;
		  m_dql.lowest_slack = UINT_MAX;
	  }
  }

  /* Enforce bounds on limit */
  /* limit = clamp(limit, dql->min_limit, dql->max_limit); */
  limit = std::min ((uint32_t)std::max (limit, m_dql.min_limit), m_dql.max_limit);

  if (limit != m_dql.limit) {
	  m_dql.limit = limit;
	  ovlimit = 0;
  }

  m_dql.adj_limit = limit + completed;
  m_dql.prev_ovlimit = ovlimit;
  m_dql.prev_last_obj_cnt = m_dql.last_obj_cnt;
  m_dql.num_completed = completed;
  m_dql.prev_num_queued = num_queued;
}

int32_t
DynamicQueueLimits::Avail () const
{
  /* return ACCESS_ONCE(dql->adj_limit) - ACCESS_ONCE(dql->num_queued); */
  return (m_dql.adj_limit) - (m_dql.num_queued);
}

void
DynamicQueueLimits::Queued (uint32_t count)
{
  /* BUG_ON(count > DQL_MAX_OBJECT); */
  NS_ASSERT (count <= DQL_MAX_OBJECT);

  m_dql.last_obj_cnt = count;

  /* We want to force a write first, so that cpu do not attempt
    * to get cache line containing last_obj_cnt, num_queued, adj_limit
    * in Shared state, but directly does a Request For Ownership
    * It is only a hint, we use barrier() only.
    */
  /* barrier(); */

  m_dql.num_queued += count;
}

} // namespace ns3
