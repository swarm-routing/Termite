// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// Termite's data path: deposit (eq. 4.1), promiscuous reception, the
// forwarded-ID duplicate list, forwarding (eq. 4.3), the dead end and
// periodic decay (eq. 4.2). Each rule names its docs/fidelity.md row.
#include "termite-router.h"

#include "swarm-selection.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace termite::core {

using swarm::core::RouteAction;
using swarm::core::WeightedHop;

bool DepositRecord::record(const PacketKey& key) {
    if (seen_.count(key) != 0) return false;
    if (seen_.size() >= capacity_) {
        seen_.erase(order_.front());
        order_.pop_front();
    }
    seen_.insert(key);
    order_.push_back(key);
    return true;
}

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
    timers_.schedule(config_.decayPeriod, [this, alive = std::weak_ptr<char>(alive_)] {
        if (alive.expired()) return;  // the router is gone
        decay();
        scheduleDecay();
    });
}

std::uint32_t Router::nextMessageId() {
    return nextMessageId_++ & ~kReplyIdBit;
}

bool Router::knows(NodeAddress destination) const {
    return !table_.entries(destination).empty();
}

Decision Router::originate(NodeAddress destination) {
    PacketHeader header;
    header.destination = destination;
    if (destination == self_) return observed(header, Decision::deliver());
    if (destination == kInvalidAddress) return observed(header, Decision::drop());
    header.source = self_;
    header.destination = destination;
    header.previousHop = self_;
    header.messageId = nextMessageId();
    header.ttl = config_.dataTtl;
    // Fidelity "ch. 4: no next hop although the destination is known": the
    // source records its own packet as forwarded, so a bounce back to it is
    // the exempt return, not a duplicate.
    return observed(header, forward(header, kInvalidAddress));
}

Decision Router::onReceive(const PacketHeader& header) {
    // A control packet is reported as received (its encoded size) once the
    // router is handed it; a data packet's header is data overhead.
    if (observer_ && header.type != PacketType::Data)
        observer_->onControlReceived(static_cast<std::uint8_t>(header.type), kPacketWireSize);
    Decision d = receive(header);
    reportState();
    if (header.type == PacketType::Data) return observed(header, d);
    // A RREQ walked on, a RREP that answers one, or a RREP routed on.
    if (d.action == RouteAction::Unicast && d.message) reportControlSent(d.message->type);
    return d;
}

Decision Router::observed(const PacketHeader& header, Decision d) {
    if (observer_ && d.action != RouteAction::None)
        observer_->onDataDecision(header.destination, d.action);
    return d;
}

Decision Router::receive(const PacketHeader& header) {
    const NodeAddress from = header.previousHop;
    // A frame naming no previous hop, source or destination describes no
    // real node: it deposits nothing and goes nowhere.
    if (from == self_ || from == kInvalidAddress || header.source == kInvalidAddress ||
        header.destination == kInvalidAddress)
        return Decision::drop();
    const bool addressed = header.nextHop == self_;
    const Key key = keyOf(header);

    // Fidelity "ch. 4: duplicate list": a copy whose ID this node has
    // forwarded is dropped without touching the table, overheard or not
    // (§4.3.9's misleading gradient). Exempt, with the bounce on: data coming
    // back from the neighbour it was forwarded to (#4, 2026-11-10).
    if (forwarded_.contains(key)) {
        if (config_.deadEndBounce && addressed && header.type == PacketType::Data &&
            forwarded_.nextHopOf(key) == from) {
            // Fidelity "ch. 4: no next hop although the destination is known":
            // the bounce lowers P(B, d) for the dead-end neighbour B. Ch. 4
            // gives no amount (eq. 4.1 only adds); repo choice (#6): the cell
            // drops to the floor, the least pheromone a known cell can hold.
            if (table_.contains(header.destination, from))
                setCell(header.destination, from, config_.pheromoneFloor);
            return forward(header, from);
        }
        return addressed ? Decision::drop() : Decision::none();
    }

    // Fidelity "ch. 4: pheromone on every packet" and "promiscuous reception":
    // every packet type deposits, at every node that hears it, once.
    depositOnce(header);
    // Fidelity "ch. 4: unknown destination": a pending destination this
    // packet made known releases its held data, received or overheard.
    releaseKnown();
    if (!addressed) return Decision::none();
    switch (header.type) {
    case PacketType::Data:
        if (header.destination == self_) return Decision::deliver();
        return forward(header, from);
    case PacketType::RouteRequest:
        return onRouteRequest(header);
    case PacketType::RouteReply:
        // Fidelity "ch. 4: route reply": consumed at the requester (its
        // deposit made the destination known), routed on elsewhere.
        if (header.destination == self_) return Decision::none();
        return forward(header, from);
    default:
        // Hello and seed are the next slice (#201).
        return Decision::none();
    }
}

Decision Router::onRouteRequest(const PacketHeader& header) {
    // Fidelity "ch. 4: which node answers a RREQ" (decided 2026-11-10, #4):
    // the node it is addressed to, if it is the destination or holds a cell
    // of the destination's column above the floor.
    if (header.destination == self_ || answers(header.destination)) {
        // Fidelity "ch. 4: route reply": it appears to come from the requested
        // destination and goes to the requester; its ID is the RREQ's with
        // the top bit set (Parameters row "RREP message ID", #200).
        PacketHeader reply;
        reply.type = PacketType::RouteReply;
        reply.source = header.destination;
        reply.destination = header.source;
        reply.previousHop = self_;
        reply.messageId = header.messageId | kReplyIdBit;
        reply.ttl = config_.rrepTtl;
        return forward(reply, kInvalidAddress);
    }
    return walk(header, header.previousHop);
}

bool Router::answers(NodeAddress destination) const {
    const auto column = table_.entries(destination);
    return std::any_of(column.begin(), column.end(),
                       [this](const auto& e) { return e.value > config_.pheromoneFloor; });
}

Decision Router::walk(PacketHeader header, NodeAddress arrival) {
    // Fidelity "ch. 4: route request": a uniform random walk, never back on
    // the arrival link, pheromone disregarded; one that cannot be forwarded
    // is dropped. One draw per hop, none for a single candidate.
    if (header.ttl == 0) return Decision::drop();
    std::vector<NodeAddress> candidates;
    for (NodeAddress n : neighbors_)
        if (n != arrival) candidates.push_back(n);
    if (candidates.empty()) return Decision::drop();
    const NodeAddress next = candidates.size() == 1
                                 ? candidates.front()
                                 : candidates[static_cast<std::size_t>(
                                       rng_.uniformInt(static_cast<int>(candidates.size())))];
    forwarded_.record(keyOf(header), next);
    header.previousHop = self_;
    header.nextHop = next;
    header.ttl = static_cast<std::uint8_t>(header.ttl - 1);
    return Decision::unicast(next, header);
}

Decision Router::hold(const PacketHeader& header, NodeAddress arrival) {
    // Fidelity "ch. 4: pending-packet buffer capacity" (repo choice, #200):
    // a packet that finds the buffer full is dropped.
    if (heldCount_ >= config_.heldCapacity) return Decision::drop();
    const bool fresh = pending_.count(header.destination) == 0;
    pending_[header.destination].held.emplace_back(header, arrival);
    ++heldCount_;
    // Fidelity "ch. 4: concurrent route requests per destination" (repo
    // choice, #200): one at a time; later data joins the held set.
    if (fresh) request(header.destination);
    return {RouteAction::Queue, header.destination, header};
}

void Router::request(NodeAddress destination) {
    Request& r = pending_[destination];
    r.generation = ++nextGeneration_;
    const std::uint64_t generation = r.generation;
    timers_.schedule(config_.rreqTimeout,
                     [this, alive = std::weak_ptr<char>(alive_), destination, generation] {
                         if (alive.expired()) return;  // the router is gone
                         onRequestTimeout(destination, generation);
                     });
    // Fidelity "ch. 4: RREQs per route request" (Table 4.1): independent walks.
    // With no neighbour none can leave (a RREQ that cannot be forwarded is
    // dropped), so none takes an ID; the request still times out. Without an
    // output port nothing could carry one either: no walk, no draw.
    if (neighbors_.empty() || !output_) return;
    for (std::size_t i = 0; i < config_.rreqsPerRequest; ++i) {
        PacketHeader rreq;
        rreq.type = PacketType::RouteRequest;
        rreq.source = self_;
        rreq.destination = destination;
        rreq.previousHop = self_;
        rreq.messageId = nextMessageId();
        rreq.ttl = config_.rreqTtl;
        const Decision d = walk(rreq, kInvalidAddress);
        if (d.action != RouteAction::Unicast) continue;
        reportControlSent(PacketType::RouteRequest);
        if (output_) output_->sendControl(d);
    }
}

void Router::releaseKnown() {
    // Collected first: forwarding a released packet changes no column, but
    // the map must not change under the loop.
    std::vector<NodeAddress> known;
    for (const auto& [destination, request] : pending_)
        if (knows(destination)) known.push_back(destination);
    for (NodeAddress destination : known) {
        auto it = pending_.find(destination);
        const auto held = std::move(it->second.held);
        pending_.erase(it);
        heldCount_ -= held.size();
        std::vector<Decision> decisions;
        for (const auto& [header, arrival] : held) {
            Decision d = forward(header, arrival);
            // A dropped one keeps the held header, so the adapter can tell
            // which held copy each decision is for (CodeRabbit on #248).
            if (!d.message) d.message = header;
            decisions.push_back(observed(header, d));
        }
        if (output_) output_->release(destination, decisions);
    }
}

void Router::onRequestTimeout(NodeAddress destination, std::uint64_t generation) {
    auto it = pending_.find(destination);
    if (it == pending_.end() || it->second.generation != generation) return;
    // Fidelity "ch. 4: unknown destination": no reply within the rreq
    // timeout, and the held data is dropped; no retry ("RREQ retries", 0).
    const std::size_t count = it->second.held.size();
    heldCount_ -= count;
    pending_.erase(it);
    if (output_) output_->discard(destination, count);
}

void Router::reportControlSent(PacketType type) {
    if (observer_)
        observer_->onControlSent(static_cast<std::uint8_t>(type), false, kPacketWireSize);
}

Decision Router::forward(PacketHeader header, NodeAddress arrival) {
    // Fidelity "ch. 4: TTL": the packet may travel no further.
    if (header.ttl == 0) return Decision::drop();
    // Fidelity "ch. 4: unknown destination": data is held for a route
    // request; control is not held (route reply row, repo choice, #200).
    if (!knows(header.destination))
        return header.type == PacketType::Data ? hold(header, arrival) : Decision::drop();

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
        // the only neighbour is the arrival link. Dropped ([RW03] §2;
        // maintainer decision 2026-11-24, #6), or, with the default-off
        // bounce, sent back on it (#4, 2026-11-10); the TTL bounds the loop.
        if (!config_.deadEndBounce || header.type != PacketType::Data) return Decision::drop();
        next = arrival;
    } else {
        next = swarm::core::rouletteSelect(candidates, rng_);
    }
    if (next == kInvalidAddress) return Decision::drop();

    forwarded_.record(keyOf(header), next);
    header.previousHop = self_;
    header.nextHop = next;
    header.ttl = static_cast<std::uint8_t>(header.ttl - 1);
    return Decision::unicast(next, header);
}

void Router::depositOnce(const PacketHeader& header) {
    // One deposit per packet per node (#4, 2026-11-10): the record holds the
    // IDs deposited for, overheard ones included (fidelity "deposited-ID
    // record size").
    if (!deposited_.record(keyOf(header))) return;
    if (!addNeighbor(header.previousHop)) return;
    // The table never holds a column for this node itself (§4.2.1).
    if (header.source == self_ || !addColumn(header.source)) return;
    // Eq. 4.1 with the ceiling of §4.2.4.
    const double value = table_.get(header.source, header.previousHop) + config_.deposit;
    setCell(header.source, header.previousHop, std::min(value, config_.pheromoneCeiling));
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
        setCell(d, neighbor, config_.initialPheromone);
    if (newColumn) {
        for (NodeAddress n : neighbors_)
            setCell(neighbor, n, config_.initialPheromone);
    }
    return true;
}

bool Router::addColumn(NodeAddress destination) {
    if (destination == self_) return false;
    if (knows(destination)) return true;
    if (table_.size() + neighbors_.size() > table_.capacity()) return false;
    for (NodeAddress n : neighbors_)
        setCell(destination, n, config_.initialPheromone);
    return true;
}

void Router::decay() {
    const double floor = config_.pheromoneFloor;
    const auto destinations = table_.destinations();
    // Fidelity "ch. 4: periodic decay", eq. 4.2, clamped at the floor (§4.2.4).
    for (NodeAddress d : destinations) {
        for (const auto& e : table_.entries(d))
            table_.set(d, e.neighbor,
                       std::max(e.value * retainPerPeriod_, floor));  // existing cell: no add
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
        eraseNeighborCells(n);
        neighbors_.erase(n);
    }
    for (NodeAddress d : destinations) {
        if (neighbors_.count(d) != 0) continue;
        const auto column = table_.entries(d);
        if (std::all_of(column.begin(), column.end(),
                        [floor](const auto& e) { return e.value <= floor; }))
            eraseColumn(d);
    }
    reportState();
}

void Router::onLinkLost(NodeAddress neighbor) {
    // Fidelity "ch. 4: a neighbour lost by communications failure".
    if (neighbors_.erase(neighbor) != 0) eraseNeighborCells(neighbor);
    reportState();
}

bool Router::setCell(NodeAddress dest, NodeAddress neighbor, double value) {
    const bool added = !table_.contains(dest, neighbor);
    if (!table_.set(dest, neighbor, value)) return false;
    if (added && observer_) observer_->onRouteChanged(dest, neighbor, true);
    return true;
}

void Router::eraseNeighborCells(NodeAddress neighbor) {
    std::vector<NodeAddress> dests;
    if (observer_) {
        for (NodeAddress d : table_.destinations())
            if (table_.contains(d, neighbor)) dests.push_back(d);
    }
    table_.eraseNeighbor(neighbor);
    for (NodeAddress d : dests)
        observer_->onRouteChanged(d, neighbor, false);
}

void Router::eraseColumn(NodeAddress dest) {
    std::vector<swarm::core::PheromoneTable::Entry> cells;
    if (observer_) cells = table_.entries(dest);
    table_.eraseDestination(dest);
    for (const auto& e : cells)
        observer_->onRouteChanged(dest, e.neighbor, false);
}

void Router::reportState() {
    if (!observer_ || table_.size() == reportedEntries_) return;
    reportedEntries_ = table_.size();
    observer_->onStateSize(static_cast<std::uint32_t>(reportedEntries_),
                           static_cast<std::uint32_t>(reportedEntries_) * kStateBytesPerEntry);
}

}  // namespace termite::core
