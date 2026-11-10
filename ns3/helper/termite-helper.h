// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
//
// Installs Termite on nodes: swarm-core's SwarmRoutingHelper for
// ns3::termite::RoutingProtocol, so `Install` also adds the link monitors that
// apply to each node.
//
//   TermiteHelper termite;
//   termite.Install(nodes);           // IPv4 stack + Termite + monitors
//   ...assign addresses...
//   termite.AssignStreams(nodes, 0);  // before Simulator::Run
#ifndef TERMITE_HELPER_H
#define TERMITE_HELPER_H

#include "ns3/swarm-routing-helper.h"

namespace ns3 {

class TermiteHelper : public SwarmRoutingHelper {
public:
    TermiteHelper();
    TermiteHelper* Copy() const override;
};

}  // namespace ns3

#endif  // TERMITE_HELPER_H
