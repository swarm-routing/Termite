// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
#include "termite-helper.h"

namespace ns3 {

TermiteHelper::TermiteHelper() : SwarmRoutingHelper("ns3::termite::RoutingProtocol") {}

TermiteHelper* TermiteHelper::Copy() const {
    return new TermiteHelper(*this);
}

}  // namespace ns3
