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

#include "measures.h"
#include "ns3/core-module.h"
#include "ns3/node.h"
// #include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
// #include "ns3/ipv4-routing-helper.h"
// #include "ns3/ipv4-address-helper.h"
// #include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-interface-container.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <iomanip>

using namespace ns3;

void
WiMeshFlowMon::Enable (double _interval, double _duration, bool _perflow, std::string _logfile)
{
  // install probes on all the nodes
  flowmon_helper->InstallAll();

  // schedule a call to UpdateStats after interval seconds
  Simulator::Schedule (Seconds(_interval), &WiMeshFlowMon::UpdateStats, this);

  interval = _interval;
  perflow = _perflow;
  logfile = _logfile;

  stringstream ss;
  ss << logfile << ".txt";

  ofstream ofs (ss.str().c_str(), ofstream::out);
  ofs << "# Time (Seconds)   Delay (MilliSeconds)   Throughput (Kbps)   PacketLoss" << endl;
  ofs.close();
}

void
WiMeshFlowMon::UpdateStats ()
{
  Ptr<FlowMonitor> flowmon = flowmon_helper->GetMonitor ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon_helper->GetClassifier ());

  flowmon->CheckForLostPackets ();

  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();

  Time time = Simulator::Now();

  ofstream ofs;

  // cumulative per-interval measures
  double delaySum = 0., avgThroughput = 0., lostPackets = 0.;
  uint32_t rxPackets = 0;

  // per-flow per-interval measures
  double flow_delaySum, flow_avgThroughput, flow_lostPackets;
  uint32_t flow_rxPackets;

  // process all the flows included in the Flow Monitor statistics and update WMstats
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); i++)
  {
    int src, dst;

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    src = (int)t.sourcePort;

    dst = (int)t.destinationPort;

    pair<int,int> sd (src, dst);

    // check whether this flow is already present in WMstats
    if (WMstats.find (sd) == WMstats.end ())
    {
      // This is a new flow

      stringstream ss;
      ss << logfile << "-" << src << "-" << dst << ".txt";

      // create a new entry in the WMstats map

      WMstats[sd] = WiMeshFlowMon::FlowStats ();
      WMstats[sd].filename = ss.str ();

      if (perflow)
      {
        // create a new file
        ofs.open (ss.str().c_str(), ofstream::out);
        ofs << "# Time (Seconds)   Delay (MilliSeconds)   Throughput (Kbps)   PacketLoss" << endl;
        ofs.close();
      }
    }

    // whether this is a new flow or not, update the curr_ fields of WMstats
    WMstats[sd].curr_delaySum += i->second.delaySum;
    WMstats[sd].curr_rxBytes += i->second.rxBytes;
    WMstats[sd].curr_rxPackets += i->second.rxPackets;
    WMstats[sd].curr_lostPackets += i->second.lostPackets;
  }

  for (map<pair<int,int>,FlowStats>::iterator wms = WMstats.begin(); wms != WMstats.end(); wms++)
  {
    // compute per-flow per-interval measures
    flow_rxPackets = wms->second.curr_rxPackets - wms->second.prev_rxPackets;
    flow_delaySum = ((double)(wms->second.curr_delaySum.GetNanoSeconds() - wms->second.prev_delaySum.GetNanoSeconds()))/1e6;
    flow_avgThroughput = (wms->second.curr_rxBytes - wms->second.prev_rxBytes)/interval*8/1000;
    flow_lostPackets = wms->second.curr_lostPackets - wms->second.prev_lostPackets;

    // update prev_ fields for this flow
    wms->second.prev_delaySum = wms->second.curr_delaySum;
    wms->second.prev_rxBytes = wms->second.curr_rxBytes;
    wms->second.prev_rxPackets = wms->second.curr_rxPackets;
    wms->second.prev_lostPackets = wms->second.curr_lostPackets;

    // reset curr_ fields for this flow
    wms->second.curr_delaySum = ns3::Time(0);
    wms->second.curr_rxBytes = 0;
    wms->second.curr_rxPackets = 0;
    wms->second.curr_lostPackets = 0;

    if (perflow)
    {
      // write per-flow measures to the file
      ofs.open (wms->second.filename.c_str(), ofstream::app);
      ofs << setw(16) << time.GetSeconds() << setw(23) << (flow_rxPackets ? flow_delaySum / flow_rxPackets : 0)
          << setw(20) << flow_avgThroughput << setw(13) << flow_lostPackets << endl;
      ofs.close();
    }

    // update cumulative per-interval measures
    rxPackets += flow_rxPackets;
    delaySum += flow_delaySum;
    avgThroughput += flow_avgThroughput;
    lostPackets += flow_lostPackets;
  }

  // write cumulative measures to the file

  stringstream ss;
  ss << logfile << ".txt";

  ofs.open (ss.str().c_str(), ofstream::app);
  ofs << setw(16) << time.GetSeconds() << setw(23) << (rxPackets ? delaySum / rxPackets : 0)
      << setw(20) << avgThroughput << setw(13) << lostPackets << endl;
  ofs.close();

  // do not update if simulation has finished
  if (!Simulator::IsFinished ())
    Simulator::Schedule (Seconds(interval), &WiMeshFlowMon::UpdateStats, this);
}

Ptr<FlowMonitor>
WiMeshFlowMon::GetFlowMonitor ()
{
  return flowmon_helper->GetMonitor ();
}
