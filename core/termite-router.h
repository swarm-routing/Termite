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

#include "swarm-observer.h"
#include "swarm-pheromone-table.h"
#include "swarm-ports.h"
#include "swarm-route-decision.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace termite::core {

using Decision = swarm::core::RouteDecision<PacketHeader>;

/// Bytes one pheromone cell takes in the state size reported to the observer:
/// the (destination, neighbour) key and the double value.
inline constexpr std::uint32_t kStateBytesPerEntry =
    static_cast<std::uint32_t>(2 * sizeof(NodeAddress) + sizeof(double));

/// The IDs this node has forwarded and the neighbour each went to: [R05]
/// §4.3.2's duplicate list, holding forwarded IDs only (maintainer decision
/// 2026-11-10, #4). Bounded, oldest forgotten first (org rule 7).
/// What identifies one packet: (source, destination, message ID). Fidelity
/// Parameters row "RREP message ID" (#200): two requesters can send RREQs
/// with the same ID for the same destination, and their RREPs share the
/// source and the ID; the destination (the requester) tells them apart. For
/// data and RREQs it adds nothing: a packet keeps its destination hop to hop.
struct PacketKey {
    NodeAddress source = kInvalidAddress;
    NodeAddress destination = kInvalidAddress;
    std::uint32_t messageId = 0;
    auto operator<=>(const PacketKey&) const = default;
};

inline PacketKey keyOf(const PacketHeader& h) {
    return {h.source, h.destination, h.messageId};
}

/// The packets this node has deposited for (fidelity "deposited-ID record
/// size"): one deposit per packet per node, overheard ones included. A
/// bounded set, oldest evicted first (org rule 7).
class DepositRecord {
public:
    explicit DepositRecord(std::size_t capacity) : capacity_(capacity < 1 ? 1 : capacity) {}
    /// True when `key` was new (and is now recorded).
    bool record(const PacketKey& key);
    std::size_t size() const { return seen_.size(); }
    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;
    std::set<PacketKey> seen_;
    std::deque<PacketKey> order_;
};

class ForwardedList {
public:
    using Key = PacketKey;

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

/// A RREP's message ID is the answered RREQ's with this bit set (fidelity
/// Parameters row "RREP message ID", repo choice, #200): its source is the
/// requested destination, whose own IDs stay below it.
inline constexpr std::uint32_t kReplyIdBit = 0x80000000U;

/// What route discovery sends besides the decision for the packet in hand
/// (#200). The adapter implements it; every call comes from inside
/// originate(), onReceive() or a timer, in a deterministic order.
class IRouterOutput {
public:
    virtual ~IRouterOutput() = default;
    /// A control packet this node originates (a RREQ): Unicast, header set.
    virtual void sendControl(const Decision& decision) = 0;
    /// `destination` became known: one decision per held packet, in the order
    /// they were held (Unicast, or Drop when one cannot be sent).
    virtual void release(NodeAddress destination, const std::vector<Decision>& decisions) = 0;
    /// The rreq timeout passed with `destination` still unknown: its `count`
    /// held packets are dropped.
    virtual void discard(NodeAddress destination, std::size_t count) = 0;
};

class Router {
public:
    /// `rng` and `timers` must outlive the router. A timer that fires after
    /// the router is gone does nothing: each callback holds a weak reference
    /// to the router's liveness token, which the destructor releases.
    Router(NodeAddress self, const Config& config, swarm::core::IRng& rng,
           swarm::core::ITimerScheduler& timers);
    ~Router() = default;
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    /// Reports to `observer` (swarm-core's IRouterObserver); null, the
    /// default, reports nothing. It must outlive the router and only reads:
    /// no decision, draw or table change depends on it (org rule 5).
    void setObserver(swarm::core::IRouterObserver* observer) { observer_ = observer; }

    /// Receives the route-discovery sends (#200). Null, the default: RREQs
    /// are not sent and held packets are released or dropped unseen. It must
    /// outlive the router.
    void setOutput(IRouterOutput* output) { output_ = output; }

    /// Starts the periodic decay (eq. 4.2): one tick every decay period.
    void start();

    /// The local transport sends a data packet to `destination`. Unicast with
    /// the header to send; Queue (header set) when the destination is unknown:
    /// the packet is held and a route request runs (#200); Drop when the held
    /// buffer is full; Deliver for this node itself.
    Decision originate(NodeAddress destination);

    /// A frame was received: addressed to this node (header.nextHop == self)
    /// or overheard (promiscuous reception, [R05] §4.2.2, p. 72). Overheard
    /// frames deposit and return None. Addressed data is delivered, forwarded,
    /// held (Queue) or dropped. An addressed RREQ returns the RREQ sent on or
    /// the RREP that answers it; an addressed RREP returns the RREP sent on,
    /// or None at the requester. Hello and seed return None.
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
    /// True while a route request for `destination` is pending.
    bool requesting(NodeAddress destination) const { return pending_.count(destination) != 0; }
    /// Data packets held for unknown destinations, all destinations.
    std::size_t held() const { return heldCount_; }

private:
    using Key = ForwardedList::Key;

    /// A pending route request: the data held for its destination (each
    /// header with the link it arrived on) and its timeout.
    struct Request {
        std::vector<std::pair<PacketHeader, NodeAddress>> held;
        std::uint64_t generation = 0;
    };

    /// Eq. 4.3 towards header.destination, never back on `arrival`. Data for
    /// an unknown destination is held; control for one is dropped.
    Decision forward(PacketHeader header, NodeAddress arrival);
    /// Holds data for an unknown destination and starts its route request.
    Decision hold(const PacketHeader& header, NodeAddress arrival);
    /// The RREQs of a new request for `destination` (Table 4.1: two walks).
    void request(NodeAddress destination);
    /// A RREQ's random walk: uniform over the neighbours but `arrival`.
    Decision walk(PacketHeader header, NodeAddress arrival);
    /// Answers or walks on an addressed RREQ.
    Decision onRouteRequest(const PacketHeader& header);
    /// The rule decided on 2026-11-10 (#4): the destination itself, or a cell
    /// of its column above the floor.
    bool answers(NodeAddress destination) const;
    /// Releases the held data of every pending destination now known.
    void releaseKnown();
    void onRequestTimeout(NodeAddress destination, std::uint64_t generation);
    /// The next message ID this node originates: the counter's low 31 bits,
    /// so it never carries kReplyIdBit and never collides with a RREP's ID.
    std::uint32_t nextMessageId();
    /// onControlSent for a RREQ or RREP this node sends (docs/observer.md).
    void reportControlSent(PacketType type);
    Decision receive(const PacketHeader& header);
    /// Reports a data decision (docs/observer.md) and returns it.
    Decision observed(const PacketHeader& header, Decision d);
    /// The table writes, each reporting an entry that appears or disappears
    /// (onRouteChanged) so the observer sees every change.
    bool setCell(NodeAddress dest, NodeAddress neighbor, double value);
    void eraseNeighborCells(NodeAddress neighbor);
    void eraseColumn(NodeAddress dest);
    /// onStateSize when the entry count differs from the last report.
    void reportState();
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
    DepositRecord deposited_;
    std::uint32_t nextMessageId_ = 0;
    double retainPerPeriod_;
    swarm::core::IRouterObserver* observer_ = nullptr;
    IRouterOutput* output_ = nullptr;
    std::size_t reportedEntries_ = 0;
    std::map<NodeAddress, Request> pending_;  // ordered: releases run in address order
    std::size_t heldCount_ = 0;
    std::uint64_t nextGeneration_ = 0;
    /// Timer callbacks capture a weak_ptr to it; destroyed with the router,
    /// so a pending timer finds it expired and returns (declared last:
    /// released first).
    std::shared_ptr<char> alive_ = std::make_shared<char>(0);
};

}  // namespace termite::core

#endif  // TERMITE_CORE_ROUTER_H
