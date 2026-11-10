// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// One router against the rows of docs/fidelity.md it implements.
#include "termite-core-tests.h"

#include <cmath>

namespace termite::core::test {

namespace {

bool near(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

/// Node 0 hears `count` frames of `source`'s via `from`, addressed to itself.
void hear(Router& r, NodeAddress source, NodeAddress from, int count, std::uint32_t& id) {
    for (int i = 0; i < count; ++i)
        r.onReceive(dataFrame(source, r.self(), from, r.self(), id++));
}

}  // namespace

void TestDepositOnReception(Checker& c) {
    Harness h(0);
    std::uint32_t id = 1;
    hear(h.router, 9, 1, 1, id);
    const auto& t = h.router.table();
    c.check(h.router.neighbors() == std::set<NodeAddress>{1}, "the previous hop becomes a row");
    c.check(near(t.get(1, 1), 1.0), "a new neighbour's column starts at the initial pheromone");
    c.check(near(t.get(9, 1), 2.0), "eq. 4.1: P(r, s) = initial + γc");
    c.check(!t.contains(0, 1), "no column for the node itself");

    hear(h.router, 2, 2, 1, id);
    c.check(near(t.get(9, 2), 1.0) && near(t.get(1, 2), 1.0),
            "a new neighbour's row covers every known column at the initial value");
    c.check(near(t.get(2, 1), 1.0) && near(t.get(2, 2), 2.0),
            "its column covers every neighbour, then takes the deposit");

    const std::size_t cells = t.size();
    c.check(h.router.onReceive(dataFrame(kInvalidAddress, 0, 1, 0, 90)).action ==
                    RouteAction::Drop &&
                h.router.onReceive(dataFrame(9, kInvalidAddress, 1, 0, 91)).action ==
                    RouteAction::Drop &&
                h.router.originate(kInvalidAddress).action == RouteAction::Drop,
            "an invalid source or destination is dropped");
    c.check(t.size() == cells && !h.router.knows(kInvalidAddress), "and creates no column for it");

    Config capped;
    capped.pheromoneCeiling = 3.0;
    Harness k(0, capped);
    hear(k.router, 9, 1, 5, id);
    c.check(near(k.router.table().get(9, 1), 3.0), "the ceiling bounds the deposit");
}

void TestPromiscuousDepositOnce(Checker& c) {
    Harness h(0);
    std::uint32_t id = 1;
    hear(h.router, 9, 2, 1, id);  // destination 9 known through 2
    const PacketHeader overheard = dataFrame(7, 9, 1, 4, 50);
    const Decision d = h.router.onReceive(overheard);
    c.check(d.action == RouteAction::None, "an overheard frame is never forwarded");
    c.check(near(h.router.table().get(7, 1), 2.0), "it deposits once");

    // The same packet, now addressed here: processed, no second deposit.
    const Decision again = h.router.onReceive(dataFrame(7, 9, 1, 0, 50));
    c.check(again.action == RouteAction::Unicast, "the first addressed copy is forwarded");
    c.check(near(h.router.table().get(7, 1), 2.0), "one deposit per packet per node");
    c.check(h.router.onReceive(overheard).action == RouteAction::None &&
                near(h.router.table().get(7, 1), 2.0),
            "a later overheard copy of a forwarded ID changes nothing");
}

void TestDuplicateListDrops(Checker& c) {
    Harness h(0);
    std::uint32_t id = 1;
    hear(h.router, 9, 1, 30, id);  // 9 through 1, overwhelmingly
    hear(h.router, 2, 2, 1, id);
    const Decision fwd = h.router.onReceive(dataFrame(7, 9, 3, 0, 5));
    c.check(fwd.action == RouteAction::Unicast && fwd.nextHop == 1, "forwarded towards 9");
    const double before = h.router.table().get(7, 2);

    const Decision dup = h.router.onReceive(dataFrame(7, 9, 2, 0, 5));
    c.check(dup.action == RouteAction::Drop, "a forwarded ID from another neighbour is dropped");
    c.check(near(h.router.table().get(7, 2), before), "without updating the table");

    // §4.3.9's misleading gradient: node 1 sends the packet on; this node
    // overhears it and must not credit 1 with a path to the source.
    const double viaOne = h.router.table().get(7, 1);
    c.check(h.router.onReceive(dataFrame(7, 9, 1, 4, 5)).action == RouteAction::None,
            "the overheard onward copy is ignored");
    c.check(near(h.router.table().get(7, 1), viaOne), "no deposit for it");
}

void TestForwardingDistribution(Checker& c) {
    Config cfg;
    cfg.sensitivity = 1.0;
    Harness h(0, cfg, 42);
    std::uint32_t id = 1;
    hear(h.router, 9, 1, 2, id);  // P(1, 9) = 3
    hear(h.router, 9, 2, 1, id);  // P(2, 9) = 2
    hear(h.router, 3, 3, 1, id);  // P(3, 9) = 1
    int counts[4] = {0, 0, 0, 0};
    const int n = 20000;
    bool oneDraw = true;
    for (int i = 0; i < n; ++i) {
        const int draws = h.rng.draws();
        const Decision d = h.router.onReceive(dataFrame(7, 9, 3, 0, 100000 + i));
        oneDraw = oneDraw && h.rng.draws() == draws + 1;
        if (d.action == RouteAction::Unicast && d.nextHop >= 0 && d.nextHop < 4)
            ++counts[d.nextHop];
    }
    c.check(counts[3] == 0, "never back on the arrival link while another neighbour exists");
    c.check(std::fabs(counts[1] / double(n) - 0.6) < 0.02 &&
                std::fabs(counts[2] / double(n) - 0.4) < 0.02,
            "eq. 4.3 with F = 1, K = 0: P ∝ pheromone (3:2)");
    c.check(oneDraw, "exactly one RNG draw per forwarding decision");

    // F = 10 sharpens 3:2 to about 57.7:1; K = 2 flattens it to (5/4)^10.
    Config sharp;
    sharp.threshold = 2.0;
    Harness k(0, sharp, 43);
    hear(k.router, 9, 1, 2, id);
    hear(k.router, 9, 2, 1, id);
    hear(k.router, 3, 3, 1, id);
    int viaTwo = 0;
    for (int i = 0; i < n; ++i)
        viaTwo += k.router.onReceive(dataFrame(7, 9, 3, 0, 200000 + i)).nextHop == 2 ? 1 : 0;
    const double expected = 1.0 / (1.0 + std::pow(5.0 / 4.0, 10.0));
    c.check(std::fabs(viaTwo / double(n) - expected) < 0.02, "K enters as (P + K)^F");
}

void TestDeadEndBounce(Checker& c) {
    Harness h(0);
    std::uint32_t id = 1;
    hear(h.router, 9, 1, 1, id);  // 1 is the only neighbour and knows of 9
    const int draws = h.rng.draws();
    const Decision d = h.router.onReceive(dataFrame(7, 9, 1, 0, 5, 20));
    c.check(d.action == RouteAction::Unicast && d.nextHop == 1,
            "no neighbour but the arrival link: back on it");
    c.check(d.message && d.message->ttl == 19 && d.message->previousHop == 0,
            "the bounce is an ordinary hop: TTL - 1, this node as previous hop");
    c.check(h.rng.draws() == draws, "a single candidate draws nothing");
}

void TestBounceIsExempt(Checker& c) {
    Harness h(0);
    std::uint32_t id = 1;
    hear(h.router, 9, 1, 30, id);  // 9 through 1, overwhelmingly
    hear(h.router, 2, 2, 1, id);
    const Decision first = h.router.onReceive(dataFrame(7, 9, 3, 0, 5));
    c.check(first.nextHop == 1, "first sent towards the strong trail");
    const double fromThree = h.router.table().get(7, 3);

    const Decision back = h.router.onReceive(dataFrame(7, 9, 1, 0, 5));
    c.check(back.action == RouteAction::Unicast && back.nextHop != 1,
            "the return from its own next hop is processed, sent to another neighbour");
    c.check(near(h.router.table().get(9, 1), h.router.config().pheromoneFloor),
            "P(B, d) towards the dead end drops to the floor (repo choice, #6)");
    c.check(near(h.router.table().get(7, 3), fromThree) && near(h.router.table().get(7, 1), 1.0),
            "the return deposits nothing");
    c.check(h.router.forwarded().nextHopOf({7, 5}) == back.nextHop,
            "the duplicate list remembers the new next hop");

    // A node whose other neighbour is its own upstream bounces further back.
    Harness k(0);
    hear(k.router, 9, 1, 1, id);
    const Decision up = k.router.onReceive(dataFrame(7, 9, 3, 0, 6));
    c.check(up.nextHop == 1, "the only way on is 1");
    const Decision further = k.router.onReceive(dataFrame(7, 9, 1, 0, 6));
    c.check(further.action == RouteAction::Unicast && further.nextHop == 3,
            "no other neighbour: further back, to the upstream node");

    // The source records its own packet as forwarded: a bounce is exempt.
    Harness s(0);
    hear(s.router, 9, 1, 30, id);
    hear(s.router, 2, 2, 1, id);
    const Decision sent = s.router.originate(9);
    c.check(sent.action == RouteAction::Unicast && sent.nextHop == 1, "the source sends to 1");
    const Decision bounced = s.router.onReceive(dataFrame(0, 9, 1, 0, sent.message->messageId));
    c.check(bounced.action == RouteAction::Unicast && bounced.nextHop == 2,
            "the bounced source sends it to its other neighbour");
}

void TestUnknownDestinationQueues(Checker& c) {
    Harness h(0);
    const Decision d = h.router.originate(9);
    c.check(d.action == RouteAction::Queue && d.nextHop == 9, "unknown destination: queued");
    c.check(d.message && d.message->source == 0 && d.message->messageId == 0 &&
                d.message->ttl == 32,
            "the queued header carries its ID and the data TTL (Table 4.1)");
    c.check(h.router.originate(9).message->messageId == 1, "message IDs count up per source");
    c.check(h.router.originate(0).action == RouteAction::Deliver, "to itself: delivered");

    std::uint32_t id = 1;
    hear(h.router, 2, 2, 1, id);
    c.check(h.router.onReceive(dataFrame(7, 9, 2, 0, 3)).action == RouteAction::Queue,
            "in transit to an unknown destination: queued too");
}

void TestTtl(Checker& c) {
    Harness h(0);
    std::uint32_t id = 1;
    hear(h.router, 9, 1, 1, id);
    hear(h.router, 2, 2, 1, id);
    c.check(h.router.onReceive(dataFrame(7, 9, 2, 0, 5, 0)).action == RouteAction::Drop,
            "TTL 0 in transit: dropped");
    const Decision last = h.router.onReceive(dataFrame(7, 9, 2, 0, 6, 1));
    c.check(last.action == RouteAction::Unicast && last.message->ttl == 0, "TTL 1: a last hop");
    c.check(h.router.onReceive(dataFrame(7, 0, 2, 0, 7, 0)).action == RouteAction::Deliver,
            "TTL 0 at the destination: delivered");
}

void TestPeriodicDecay(Checker& c) {
    Harness h(0);
    std::uint32_t id = 1;
    hear(h.router, 2, 2, 10, id);  // P(2, 2) = 11: outlives everything below
    hear(h.router, 5, 1, 1, id);   // P(1, 5) = 2, P(2, 5) = 1
    h.router.start();
    const auto& t = h.router.table();
    h.events.runUntil(0.999);
    c.check(near(t.get(5, 1), 2.0), "nothing decays within the first period");
    h.events.runUntil(1.0);
    c.check(near(t.get(5, 1), 2.0 * std::exp(-0.105)), "eq. 4.2: e^(-τ) once per period");
    h.events.runUntil(22.0);
    c.check(near(t.get(1, 1), 0.1), "no cell decays below the floor");
    c.check(h.router.neighbors().count(1) == 1, "a row with a live cell keeps its neighbour");
    h.events.runUntil(28.0);
    c.check(h.router.knows(5), "a column with a live cell stays");
    h.events.runUntil(29.0);
    c.check(h.router.neighbors() == std::set<NodeAddress>{2},
            "a decayed row removes the neighbour");
    c.check(!h.router.knows(5) && !h.router.knows(1),
            "decayed columns go, a neighbour's once its row has gone");
    c.check(h.router.knows(2), "the live column stays");
    c.check(h.events.pending() == 1, "exactly one decay tick pending");
}

void TestLinkLost(Checker& c) {
    Harness h(0);
    std::uint32_t id = 1;
    hear(h.router, 9, 1, 1, id);
    hear(h.router, 2, 2, 1, id);
    h.router.onLinkLost(1);
    c.check(h.router.neighbors() == std::set<NodeAddress>{2}, "the row goes at once");
    c.check(!h.router.table().contains(9, 1) && h.router.table().contains(9, 2),
            "every cell through it goes, the column stays");
    h.router.onLinkLost(2);
    c.check(h.router.table().empty(), "the last neighbour lost: an empty table");
}

void TestBoundedStructures(Checker& c) {
    ForwardedList list(64);
    for (std::uint32_t i = 0; i < 65; ++i)
        list.record({7, i}, 1);
    c.check(list.size() == 64 && !list.contains({7, 0}) && list.contains({7, 64}),
            "the duplicate list holds 64, oldest evicted first");
    list.record({7, 1}, 3);
    c.check(list.size() == 64 && list.nextHopOf({7, 1}) == 3, "re-recording updates in place");

    Config cfg;
    cfg.tableCapacity = 4;
    Harness h(0, cfg);
    std::uint32_t id = 1;
    hear(h.router, 1, 1, 1, id);
    hear(h.router, 2, 2, 1, id);
    c.check(h.router.table().size() == 4, "two neighbours fill four cells");
    hear(h.router, 3, 3, 1, id);
    c.check(h.router.table().size() == 4 && h.router.neighbors().size() == 2,
            "a neighbour the table cannot hold is refused whole");

    Config small;
    small.depositRecordSize = 2;
    Harness k(0, small);
    for (std::uint32_t i = 0; i < 3; ++i)
        k.router.onReceive(dataFrame(7, 9, 1, 4, i));
    k.router.onReceive(dataFrame(7, 9, 1, 4, 0));
    c.check(near(k.router.table().get(7, 1), 5.0), "the deposit record is bounded too");
}

}  // namespace termite::core::test
