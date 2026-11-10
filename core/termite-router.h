// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// One Termite node: its pheromone table and its decisions for data packets,
// ch. 4 of Roth's dissertation ([R05] §4.2–§4.3) with the maintainer's
// 2026-11-10 decisions recorded in docs/fidelity.md (#4). Written fresh from
// the dissertation and the sheet (golden rule 13).
//
// The router is pure (golden rule 10): randomness comes through IRng, the
// decay tick through ITimerScheduler (swarm-core's ports), and every packet
// decision goes back to the adapter as a RouteDecision carrying the header to
// send. The table is swarm-core's PheromoneTable, keyed (destination,
// neighbour) as [R05] §4.2.1's rows and columns; the next hop is drawn with
// swarm-core's rouletteSelect.
//
// Not in this slice: route requests and replies, hello, seed (decoded and
// depositing, not yet acted on), resending a packet whose next hop failed.
#ifndef TERMITE_CORE_ROUTER_H
#define TERMITE_CORE_ROUTER_H

#include "termite-config.h"
#include "termite-packet.h"

#include "swarm-dedup-history.h"
#include "swarm-pheromone-table.h"
#include "swarm-ports.h"
#include "swarm-route-decision.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <utility>

namespace termite::core {

using Decision = swarm::core::RouteDecision<PacketHeader>;

/// The IDs this node has forwarded and the neighbour each went to: [R05]
/// §4.3.2's duplicate list, holding forwarded IDs only (maintainer decision
/// 2026-11-10, #4). Bounded, oldest forgotten first (org rule 7).
class ForwardedList {
public:
    using Key = std::pair<NodeAddress, std::uint32_t>;  // (source, message ID)

    explicit ForwardedList(std::size_t capacity) : capacity_(capacity < 1 ? 1 : capacity) {}

    /// Records that `key` went to `nextHop`; a key already held keeps its age
    /// and takes the new next hop (a bounced packet sent on again).
    void record(const Key& key, NodeAddress nextHop);
    /// The neighbour `key` was last sent to, or kInvalidAddress if not held.
    NodeAddress nextHopOf(const Key& key) const;
    bool contains(const Key& key) const { return nextHop_.count(key) != 0; }

    std::size_t size() const { return nextHop_.size(); }
    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;
    std::map<Key, NodeAddress> nextHop_;
    std::deque<Key> order_;  // insertion order, for FIFO eviction
};

class Router {
public:
    /// `rng` and `timers` must outlive the router.
    Router(NodeAddress self, const Config& config, swarm::core::IRng& rng,
           swarm::core::ITimerScheduler& timers);

    /// Starts the periodic decay (eq. 4.2): one tick every decay period.
    void start();

    /// The local transport sends a data packet to `destination`. Unicast with
    /// the header to send; Queue (header set) when the destination is unknown,
    /// for the route request of the next slice; Deliver for this node itself.
    Decision originate(NodeAddress destination);

    /// A frame was received: addressed to this node (header.nextHop == self)
    /// or overheard (promiscuous reception, [R05] §4.2.2, p. 72). Overheard
    /// frames deposit and return None; addressed data is delivered, forwarded,
    /// queued or dropped.
    Decision onReceive(const PacketHeader& header);

    /// A neighbour was lost by communications failure: its row goes at once
    /// ([R05] §4.2.3, p. 73).
    void onLinkLost(NodeAddress neighbor);

    /// One decay period: eq. 4.2 on every cell, then the removals of §4.2.3.
    /// start() calls it on the timer; public for the adapter and the tests.
    void decay();

    NodeAddress self() const { return self_; }
    const Config& config() const { return config_; }
    const swarm::core::PheromoneTable& table() const { return table_; }
    const std::set<NodeAddress>& neighbors() const { return neighbors_; }
    const ForwardedList& forwarded() const { return forwarded_; }
    /// True when the table holds a column for `destination`.
    bool knows(NodeAddress destination) const;

private:
    using Key = ForwardedList::Key;

    Decision forward(PacketHeader header, NodeAddress arrival);
    /// Eq. 4.1 once per packet per node: P(previousHop, source) += γc.
    void depositOnce(const PacketHeader& header);
    bool addNeighbor(NodeAddress neighbor);
    bool addColumn(NodeAddress destination);
    void scheduleDecay();

    NodeAddress self_;
    Config config_;
    swarm::core::IRng& rng_;
    swarm::core::ITimerScheduler& timers_;
    swarm::core::PheromoneTable table_;
    std::set<NodeAddress> neighbors_;
    ForwardedList forwarded_;
    swarm::core::DedupHistory deposited_;
    std::uint32_t nextMessageId_ = 0;
    double retainPerPeriod_;
};

}  // namespace termite::core

#endif  // TERMITE_CORE_ROUTER_H
