/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Universita' degli Studi di Napoli Federico II
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

/*
 * Measures average packet delay, average throughput and number of lost packets over
 * time intervals of the specified duration for each single end-to-end flow (if perflow
 * is true) and for the whole set of flows.
 * NOTE the statistics are collected at the network layer, hence a transmitted packet is
 * assumed to be lost if it has "not been reportedly received or forwarded" for a specified
 * time (by default, 10 seconds).
 */

#ifndef MEASURES_H
#define MEASURES_H

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h" 
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-interface-container.h"
#include <string>

using namespace std;
using namespace ns3;

class WiMeshFlowMon
{
public:
  // constructor
  WiMeshFlowMon () { flowmon_helper = new FlowMonitorHelper; }

  // Schedules periodic calls (every _interval seconds) to UpdateStats
  // If perflow is true, statistics are collected for every single flow
  void Enable (double _interval, double _duration, bool _perflow, string _logfile);

  // destructor
  ~WiMeshFlowMon () { delete flowmon_helper; }

  Ptr<FlowMonitor> GetFlowMonitor ();

private:
  // Cumulative (since the beginning of the simulation) per-flow measures
  // prev_ fields contain values up to the previous interval
  // curr_ fields contain values up to the current interval
  // This is needed to handle TCP, where two flows (data and ack) map to the same WiMesh flow
  // Have the same meaning as in FlowMonitor::FlowStats
  struct FlowStats
  {
    ns3::Time prev_delaySum, curr_delaySum;
    uint64_t prev_rxBytes, curr_rxBytes;
    uint32_t prev_rxPackets, curr_rxPackets;
    uint32_t prev_lostPackets, curr_lostPackets;
    string filename;

    FlowStats ()
    : prev_delaySum (ns3::Time(0)),
      curr_delaySum (ns3::Time(0)),
      prev_rxBytes (0),
      curr_rxBytes (0),
      prev_rxPackets (0),
      curr_rxPackets (0),
      prev_lostPackets (0),
      curr_lostPackets (0)
      {}
  };

  map<pair<int,int>,FlowStats> WMstats;

  void UpdateStats ();

  double interval;
  bool perflow;
  string logfile;
  // copying a flowmon_helper is forbidden, hence use a pointer
  FlowMonitorHelper* flowmon_helper;
};

#endif /* MEASURES_H */
