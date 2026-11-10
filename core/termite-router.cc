// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// Termite's data path: deposit (eq. 4.1), promiscuous reception, the
// forwarded-ID duplicate list, forwarding (eq. 4.3), the dead-end bounce and
// periodic decay (eq. 4.2). Each rule names its docs/fidelity.md row.
#include "termite-router.h"

#include "swarm-selection.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace termite::core {

using swarm::core::RouteAction;
using swarm::core::WeightedHop;

void ForwardedList::record(const Key& key, NodeAddress nextHop) {
    auto it = nextHop_.find(key);
    if (it != nextHop_.end()) {
        it->second = nextHop;
        return;
    }
    if (nextHop_.size() >= capacity_) {
        nextHop_.erase(order_.front());
        order_.pop_front();
    }
    nextHop_.emplace(key, nextHop);
    order_.push_back(key);
}

NodeAddress ForwardedList::nextHopOf(const Key& key) const {
    auto it = nextHop_.find(key);
    return it == nextHop_.end() ? kInvalidAddress : it->second;
}

Router::Router(NodeAddress self, const Config& config, swarm::core::IRng& rng,
               swarm::core::ITimerScheduler& timers)
    : self_(self), config_(config), rng_(rng), timers_(timers), table_(config.tableCapacity),
      forwarded_(config.duplicateListSize), deposited_(config.depositRecordSize),
      retainPerPeriod_(std::exp(-config.decayRate)) {}

void Router::start() {
    scheduleDecay();
}

void Router::scheduleDecay() {
    if (!(config_.decayPeriod > 0.0)) return;
    timers_.schedule(config_.decayPeriod, [this] {
        decay();
        scheduleDecay();
    });
}

bool Router::knows(NodeAddress destination) const {
    return !table_.entries(destination).empty();
}

Decision Router::originate(NodeAddress destination) {
    if (destination == self_) return Decision::deliver();
    PacketHeader header;
    header.source = self_;
    header.destination = destination;
    header.previousHop = self_;
    header.messageId = nextMessageId_++;
    header.ttl = config_.dataTtl;
    // Fidelity "ch. 4: no next hop although the destination is known": the
    // source records its own packet as forwarded, so a bounce back to it is
    // the exempt return, not a duplicate.
    return forward(header, kInvalidAddress);
}

Decision Router::onReceive(const PacketHeader& header) {
    const NodeAddress from = header.previousHop;
    if (from == self_ || from == kInvalidAddress) return Decision::drop();
    const bool addressed = header.nextHop == self_;
    const Key key{header.source, header.messageId};

    // Fidelity "ch. 4: duplicate list": a copy whose ID this node has
    // forwarded is dropped without touching the table, overheard or not
    // (§4.3.9's misleading gradient). Exempt: data coming back from the
    // neighbour it was forwarded to, the dead-end bounce (#4, 2026-11-10).
    if (forwarded_.contains(key)) {
        if (addressed && header.type == PacketType::Data && forwarded_.nextHopOf(key) == from) {
            // Fidelity "ch. 4: no next hop although the destination is known":
            // the bounce lowers P(B, d) for the dead-end neighbour B. Ch. 4
            // gives no amount (eq. 4.1 only adds); repo choice (#6): the cell
            // drops to the floor, the least pheromone a known cell can hold.
            if (table_.contains(header.destination, from))
                table_.set(header.destination, from, config_.pheromoneFloor);
            return forward(header, from);
        }
        return addressed ? Decision::drop() : Decision::none();
    }

    // Fidelity "ch. 4: pheromone on every packet" and "promiscuous reception":
    // every packet type deposits, at every node that hears it, once.
    depositOnce(header);
    if (!addressed) return Decision::none();
    // Hello, seed, RREQ and RREP handling is the next slice.
    if (header.type != PacketType::Data) return Decision::none();
    if (header.destination == self_) return Decision::deliver();
    return forward(header, from);
}

Decision Router::forward(PacketHeader header, NodeAddress arrival) {
    // Fidelity "ch. 4: TTL": the packet may travel no further.
    if (header.ttl == 0) return Decision::drop();
    // Fidelity "ch. 4: unknown destination": buffered for a route request.
    if (!knows(header.destination)) return {RouteAction::Queue, header.destination, header};

    // Fidelity "ch. 4: forwarding", eq. 4.3: P(n) ∝ (P(n, d) + K)^F over the
    // neighbours except the arrival link r. Weights are taken relative to the
    // best candidate, the same distribution without overflow at a large F.
    const auto entries = table_.entries(header.destination);
    double best = 0.0;
    for (const auto& e : entries) {
        if (e.neighbor != arrival) best = std::max(best, e.value + config_.threshold);
    }
    std::vector<WeightedHop> candidates;
    for (const auto& e : entries) {
        if (e.neighbor == arrival) continue;
        candidates.push_back(
            {e.neighbor, std::pow((e.value + config_.threshold) / best, config_.sensitivity)});
    }
    NodeAddress next = kInvalidAddress;
    if (candidates.empty()) {
        // Fidelity "ch. 4: no next hop although the destination is known":
        // the only neighbour is the arrival link, so the packet goes back on
        // it (maintainer decision 2026-11-10, #4); the TTL bounds the loop.
        next = arrival;
    } else {
        next = swarm::core::rouletteSelect(candidates, rng_);
    }
    if (next == kInvalidAddress) return Decision::drop();

    forwarded_.record({header.source, header.messageId}, next);
    header.previousHop = self_;
    header.nextHop = next;
    header.ttl = static_cast<std::uint8_t>(header.ttl - 1);
    return Decision::unicast(next, header);
}

void Router::depositOnce(const PacketHeader& header) {
    // One deposit per packet per node (#4, 2026-11-10): the record holds the
    // IDs deposited for, overheard ones included (fidelity "deposited-ID
    // record size").
    if (!deposited_.record(header.source, header.messageId)) return;
    if (!addNeighbor(header.previousHop)) return;
    // The table never holds a column for this node itself (§4.2.1).
    if (header.source == self_ || !addColumn(header.source)) return;
    // Eq. 4.1 with the ceiling of §4.2.4.
    const double value = table_.get(header.source, header.previousHop) + config_.deposit;
    table_.set(header.source, header.previousHop, std::min(value, config_.pheromoneCeiling));
}

bool Router::addNeighbor(NodeAddress neighbor) {
    if (neighbor == self_ || neighbor == kInvalidAddress) return false;
    if (neighbors_.count(neighbor) != 0) return true;
    // Fidelity "ch. 4: bounds": a new neighbour adds a row (a cell in every
    // known column) and, unless it is already a destination, a column, all at
    // the initial pheromone. Refused whole when the table cannot hold it.
    const auto destinations = table_.destinations();
    const bool newColumn =
        std::find(destinations.begin(), destinations.end(), neighbor) == destinations.end();
    const std::size_t needed = destinations.size() + (newColumn ? neighbors_.size() + 1 : 0);
    if (table_.size() + needed > table_.capacity()) return false;
    neighbors_.insert(neighbor);
    for (NodeAddress d : destinations)
        table_.set(d, neighbor, config_.initialPheromone);
    if (newColumn) {
        for (NodeAddress n : neighbors_)
            table_.set(neighbor, n, config_.initialPheromone);
    }
    return true;
}

bool Router::addColumn(NodeAddress destination) {
    if (destination == self_) return false;
    if (knows(destination)) return true;
    if (table_.size() + neighbors_.size() > table_.capacity()) return false;
    for (NodeAddress n : neighbors_)
        table_.set(destination, n, config_.initialPheromone);
    return true;
}

void Router::decay() {
    const double floor = config_.pheromoneFloor;
    const auto destinations = table_.destinations();
    // Fidelity "ch. 4: periodic decay", eq. 4.2, clamped at the floor (§4.2.4).
    for (NodeAddress d : destinations) {
        for (const auto& e : table_.entries(d))
            table_.set(d, e.neighbor, std::max(e.value * retainPerPeriod_, floor));
    }
    // Fidelity "ch. 4: a destination column is removed ...": a row all at the
    // floor removes the neighbour; a column all at the floor goes, but a
    // neighbour's column only once its row has gone too.
    std::vector<NodeAddress> decayedRows;
    for (NodeAddress n : neighbors_) {
        const bool decayed = std::all_of(destinations.begin(), destinations.end(),
                                         [&](NodeAddress d) { return table_.get(d, n) <= floor; });
        if (decayed) decayedRows.push_back(n);
    }
    for (NodeAddress n : decayedRows) {
        table_.eraseNeighbor(n);
        neighbors_.erase(n);
    }
    for (NodeAddress d : destinations) {
        if (neighbors_.count(d) != 0) continue;
        const auto column = table_.entries(d);
        if (std::all_of(column.begin(), column.end(),
                        [floor](const auto& e) { return e.value <= floor; }))
            table_.eraseDestination(d);
    }
}

void Router::onLinkLost(NodeAddress neighbor) {
    // Fidelity "ch. 4: a neighbour lost by communications failure".
    if (neighbors_.erase(neighbor) != 0) table_.eraseNeighbor(neighbor);
}

}  // namespace termite::core
