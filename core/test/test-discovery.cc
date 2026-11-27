// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// Route discovery (#200): RREQ origination and its random walk, the answer
// rule, the RREP, held data released or dropped at the rreq timeout, and
// delivery to a destination that never sent to the requester.
#include "termite-core-tests.h"

#include <algorithm>
#include <cmath>

namespace termite::core::test {

namespace {

/// Records what a router sends through its output port.
struct Sink : IRouterOutput {
    std::vector<Decision> control;
    std::vector<std::pair<NodeAddress, std::vector<Decision>>> released;
    std::vector<std::pair<NodeAddress, std::size_t>> discarded;
    void sendControl(const Decision& d) override { control.push_back(d); }
    void release(NodeAddress dest, const std::vector<Decision>& ds) override {
        released.emplace_back(dest, ds);
    }
    void discard(NodeAddress dest, std::size_t count) override {
        discarded.emplace_back(dest, count);
    }
};

bool near(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

/// Node 0 hears one frame of `source`'s via `from`, addressed to itself.
void hearOnce(Router& r, NodeAddress source, NodeAddress from, std::uint32_t id) {
    r.onReceive(dataFrame(source, r.self(), from, r.self(), id));
}

PacketHeader rreq(NodeAddress requester, NodeAddress sought, NodeAddress from, NodeAddress to,
                  std::uint32_t id, std::uint8_t ttl = 32) {
    PacketHeader h = dataFrame(requester, sought, from, to, id, ttl);
    h.type = PacketType::RouteRequest;
    return h;
}

}  // namespace

void TestRouteRequestOrigination(Checker& c) {
    Harness h(0);
    Sink sink;
    h.router.setOutput(&sink);
    hearOnce(h.router, 1, 1, 1);
    hearOnce(h.router, 2, 2, 1);
    hearOnce(h.router, 3, 3, 1);

    const Decision d = h.router.originate(9);
    c.check(d.action == RouteAction::Queue && d.message && d.message->messageId == 0,
            "unknown destination: the data is held (Queue)");
    c.check(h.router.requesting(9) && h.router.held() == 1, "a request is pending, one held");
    c.check(sink.control.size() == 2, "two RREQs per request (Table 4.1)");
    for (const Decision& r : sink.control) {
        c.check(r.action == RouteAction::Unicast && r.message &&
                    r.message->type == PacketType::RouteRequest && r.message->source == 0 &&
                    r.message->destination == 9 && r.message->previousHop == 0 &&
                    r.message->ttl == 31 && r.nextHop >= 1 && r.nextHop <= 3,
                "a RREQ: requester as source, sought node as destination, TTL 32 - 1");
    }
    c.check(sink.control[0].message->messageId == 1 && sink.control[1].message->messageId == 2,
            "each walk has its own message ID");

    const Decision again = h.router.originate(9);
    c.check(again.action == RouteAction::Queue && sink.control.size() == 2 && h.router.held() == 2,
            "one request per destination: more data joins it, no new RREQ");

    // With no neighbour no walk can leave, and no ID is taken.
    Harness alone(0);
    Sink quiet;
    alone.router.setOutput(&quiet);
    alone.router.originate(9);
    c.check(quiet.control.empty() && alone.router.originate(9).message->messageId == 1,
            "an isolated node sends no RREQ and takes no ID for one");
}

void TestRouteRequestWalk(Checker& c) {
    Harness h(0);
    hearOnce(h.router, 1, 1, 1);
    hearOnce(h.router, 2, 2, 1);
    hearOnce(h.router, 3, 3, 1);
    int seen[4] = {0, 0, 0, 0};
    bool oneDraw = true;
    for (std::uint32_t i = 0; i < 3000; ++i) {
        const int draws = h.rng.draws();
        const Decision d = h.router.onReceive(rreq(7, 9, 1, 0, 100 + i));
        oneDraw = oneDraw && h.rng.draws() == draws + 1;
        if (d.action == RouteAction::Unicast && d.nextHop >= 0 && d.nextHop < 4) ++seen[d.nextHop];
    }
    c.check(seen[1] == 0, "never back on the arrival link");
    c.check(seen[2] > 1300 && seen[3] > 1300, "uniform over the others, pheromone ignored");
    c.check(oneDraw, "one draw per hop");

    h.router.onReceive(rreq(7, 9, 1, 0, 60000));
    const Decision repeat = h.router.onReceive(rreq(7, 9, 2, 0, 60000));
    c.check(repeat.action == RouteAction::Drop,
            "a RREQ this node forwarded before is dropped (duplicate list)");
    c.check(h.router.onReceive(rreq(7, 9, 1, 0, 50000, 0)).action == RouteAction::Drop,
            "TTL 0: dropped");

    Harness line(0);
    hearOnce(line.router, 1, 1, 1);
    hearOnce(line.router, 2, 2, 1);
    const int draws = line.rng.draws();
    const Decision single = line.router.onReceive(rreq(7, 9, 1, 0, 5));
    c.check(single.nextHop == 2 && line.rng.draws() == draws, "one candidate draws nothing");
    Harness end(0);
    hearOnce(end.router, 1, 1, 1);
    c.check(end.router.onReceive(rreq(7, 9, 1, 0, 5)).action == RouteAction::Drop,
            "a RREQ that cannot be forwarded is dropped");
    c.check(end.router.onReceive(rreq(7, 9, 1, 4, 6)).action == RouteAction::None,
            "an overheard RREQ is not answered or walked");
}

void TestRouteRequestAnswer(Checker& c) {
    // Node 0 knows 9 through 2, above the floor; the RREQ of 7 arrives from 1.
    Harness h(0);
    hearOnce(h.router, 7, 1, 1);
    hearOnce(h.router, 9, 2, 1);
    const Decision d = h.router.onReceive(rreq(7, 9, 1, 0, 40));
    c.check(d.action == RouteAction::Unicast && d.message &&
                d.message->type == PacketType::RouteReply,
            "a node with a cell above the floor answers");
    c.check(d.message->source == 9 && d.message->destination == 7 &&
                d.message->messageId == (40U | kReplyIdBit) && d.message->ttl == 31,
            "the RREP appears to come from 9, goes to 7, ID with the top bit");
    c.check(d.nextHop == 1, "towards the requester by eq. 4.3 (the arrival link is allowed)");

    // The destination itself answers.
    Harness dest(9);
    hearOnce(dest.router, 7, 1, 1);
    const Decision self = dest.router.onReceive(rreq(7, 9, 1, 9, 41));
    c.check(self.action == RouteAction::Unicast && self.message->source == 9 &&
                self.message->type == PacketType::RouteReply,
            "the destination answers");

    // A column at the floor carries no route: the RREQ walks on. With the
    // initial pheromone at the floor, 9's column (learnt only as a
    // neighbour, never deposited into) holds the floor in every cell.
    Config low;
    low.initialPheromone = low.pheromoneFloor;
    Harness floor(0, low);
    hearOnce(floor.router, 3, 9, 1);  // 9 relays 3's packet: 9 is a neighbour
    hearOnce(floor.router, 7, 1, 1);
    c.check(floor.router.knows(9), "9's column exists");
    const Decision w = floor.router.onReceive(rreq(7, 9, 1, 0, 42));
    c.check(w.action == RouteAction::Unicast && w.message->type == PacketType::RouteRequest &&
                w.nextHop == 9,
            "a column at the floor does not answer: the walk goes on");
}

void TestRouteReplyReleases(Checker& c) {
    Harness h(0);
    Sink sink;
    h.router.setOutput(&sink);
    hearOnce(h.router, 1, 1, 1);
    hearOnce(h.router, 2, 2, 1);
    h.router.originate(9);
    h.router.originate(9);
    c.check(h.router.held() == 2, "two held for 9");

    // The RREP from 9, via 2: it deposits for 9, which releases the held data.
    PacketHeader rrep = dataFrame(9, 0, 2, 0, 5 | kReplyIdBit, 30);
    rrep.type = PacketType::RouteReply;
    const Decision d = h.router.onReceive(rrep);
    c.check(d.action == RouteAction::None, "the RREP is consumed at the requester");
    c.check(sink.released.size() == 1 && sink.released[0].first == 9 &&
                sink.released[0].second.size() == 2,
            "both held packets are released, once");
    bool viaTwo = true;
    for (const Decision& r : sink.released[0].second)
        viaTwo = viaTwo && r.action == RouteAction::Unicast && r.nextHop == 2 &&
                 r.message->ttl == 31 && r.message->previousHop == 0;
    c.check(viaTwo, "released by eq. 4.3 along the new trail, TTL - 1");
    c.check(!h.router.requesting(9) && h.router.held() == 0, "the request ends");
    h.events.runUntil(5.0);
    c.check(sink.discarded.empty(), "a request that ended does not time out");

    // A relay's held packet whose release is a dead end: the Drop carries the
    // held header, so the adapter can tell which held copy it is for.
    Harness deadEnd(0);
    Sink out;
    deadEnd.router.setOutput(&out);
    const PacketHeader in = dataFrame(5, 9, 1, 0, 44, 20);
    c.check(deadEnd.router.onReceive(in).action == RouteAction::Queue, "held at the relay");
    hearOnce(deadEnd.router, 9, 1, 2);  // 9 known, but only through the arrival link
    c.check(out.released.size() == 1 && out.released[0].second.size() == 1, "released");
    const Decision& drop = out.released[0].second.front();
    c.check(drop.action == RouteAction::Drop && drop.message && *drop.message == in,
            "a released Drop carries the held header");

    // An intermediate node routes the RREP on; with no column for the
    // requester it drops it (control is not held).
    Harness mid(0);
    hearOnce(mid.router, 7, 1, 1);
    hearOnce(mid.router, 3, 3, 1);
    PacketHeader on = dataFrame(9, 7, 3, 0, 6 | kReplyIdBit, 30);
    on.type = PacketType::RouteReply;
    const Decision fwd = mid.router.onReceive(on);
    c.check(fwd.action == RouteAction::Unicast && fwd.nextHop == 1 &&
                fwd.message->type == PacketType::RouteReply,
            "an intermediate node routes the RREP on, towards the requester");
    c.check(mid.router.knows(9), "and learns the requested destination on the way");
    PacketHeader lost = dataFrame(9, 8, 3, 0, 7 | kReplyIdBit, 30);
    lost.type = PacketType::RouteReply;
    c.check(mid.router.onReceive(lost).action == RouteAction::Drop,
            "no column for the requester: dropped, not held");
    c.check(mid.router.held() == 0, "control is never held");

    // Two requesters, 1 and 2, whose RREQs for 9 had the same ID: their
    // RREPs share source and ID, and the relay carries and deposits both,
    // since the key includes the destination (#200, found on #220).
    Harness relay(0);
    hearOnce(relay.router, 1, 1, 1);
    hearOnce(relay.router, 2, 2, 1);
    hearOnce(relay.router, 3, 3, 1);
    PacketHeader toOne = dataFrame(9, 1, 3, 0, 5 | kReplyIdBit, 30);
    toOne.type = PacketType::RouteReply;
    PacketHeader toTwo = toOne;
    toTwo.destination = 2;
    const Decision first = relay.router.onReceive(toOne);
    const double once = relay.router.table().get(9, 3);
    const Decision second = relay.router.onReceive(toTwo);
    c.check(first.action == RouteAction::Unicast && first.nextHop == 1 &&
                second.action == RouteAction::Unicast && second.nextHop == 2,
            "both RREPs are routed on, each to its requester");
    c.check(near(relay.router.table().get(9, 3), once + relay.router.config().deposit),
            "and each deposits for 9");
}

void TestHeldTimeoutAndCap(Checker& c) {
    Config small;
    small.heldCapacity = 2;
    Harness h(0, small);
    Sink sink;
    h.router.setOutput(&sink);
    hearOnce(h.router, 1, 1, 1);
    h.router.originate(9);
    h.router.originate(9);
    c.check(h.router.originate(8).action == RouteAction::Drop,
            "a packet that finds the buffer full is dropped");
    c.check(!h.router.requesting(8), "and starts no request");
    h.events.runUntil(1.9);
    c.check(sink.discarded.empty(), "held until the rreq timeout");
    h.events.runUntil(2.1);
    c.check(sink.discarded.size() == 1 && sink.discarded[0] == std::pair{NodeAddress{9}, 2UL},
            "at 2 s (Table 4.1) both are dropped");
    c.check(!h.router.requesting(9) && h.router.held() == 0, "no retry: the request ends");
    c.check(h.router.originate(9).action == RouteAction::Queue && h.router.requesting(9),
            "a later packet starts a new request");

    // A router destroyed with its timeout and decay still pending: the
    // timers fire on nothing (ASan would catch a use after free).
    EventQueue events;
    SeededRng rng(1);
    {
        Router gone(0, Config{}, rng, events);
        Sink quiet;
        gone.setOutput(&quiet);
        gone.start();
        hearOnce(gone, 1, 1, 1);
        gone.originate(9);
        c.check(gone.requesting(9), "a request and a decay are pending");
    }
    events.runUntil(10.0);
    c.check(true, "the pending timers of a destroyed router do nothing");
}

namespace {

/// Each node has heard one frame from each neighbour, and nothing more: the
/// neighbour discovery hello does in a fresh network (#201).
void knowNeighbours(Network& net, const std::vector<std::pair<NodeAddress, NodeAddress>>& links) {
    for (const auto& [a, b] : links) {
        net.addLink(a, b);
        net.layTrail({a, b});
        net.layTrail({b, a});
    }
}

}  // namespace

void TestDiscoveryEndToEnd(Checker& c) {
    // A chain where 0 has never heard from 4, nor 1 and 2 of it.
    Network chain(3);
    knowNeighbours(chain, {{0, 1}, {1, 2}, {2, 3}, {3, 4}});
    c.check(!chain.node(0).knows(4) && !chain.node(1).knows(4), "4 is unknown to 0 and 1");
    chain.send(0, 4);
    chain.events().runUntil(1.0);
    c.check(chain.delivered() == 1, "chain: 0 delivers to 4, which never sent to it");
    c.check(chain.controlSent() == 2, "two RREQs left the requester");

    // The diamond of the multipath test: 0 - {1, 2} - 3.
    Network diamond(5);
    knowNeighbours(diamond, {{0, 1}, {0, 2}, {1, 3}, {2, 3}});
    c.check(!diamond.node(0).knows(3), "3 is unknown to 0");
    for (int i = 0; i < 4; ++i)
        diamond.send(0, 3);
    diamond.events().runUntil(1.0);
    c.check(diamond.delivered() == 4, "diamond: every held packet delivered once found");
    c.check(diamond.queued() == 0 && diamond.dropped() == 0, "none left held, none dropped");
    diamond.send(3, 0);
    diamond.events().runUntil(2.0);
    c.check(diamond.delivered() == 5, "and back, on the trail the exchange laid");
}

}  // namespace termite::core::test
