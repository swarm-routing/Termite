// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// The router reports through swarm-core's IRouterObserver (docs/observer.md,
// #41): every callback is driven with a recording observer, and a run is
// identical with and without one (org rule 5).
#include "termite-core-tests.h"

#include <algorithm>

namespace termite::core::test {

namespace {

/// Records every event in order.
struct Recorder : swarm::core::IRouterObserver {
    struct Route {
        NodeAddress dest, next;
        bool added;
        bool operator==(const Route&) const = default;
    };
    struct Control {
        std::uint8_t kind;
        std::uint32_t bytes;
    };
    std::vector<Control> sent, received;
    std::vector<Route> routes;
    std::vector<std::pair<NodeAddress, RouteAction>> decisions;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> states;

    void onControlSent(std::uint8_t kind, bool, std::uint32_t bytes) override {
        sent.push_back({kind, bytes});
    }
    void onControlReceived(std::uint8_t kind, std::uint32_t bytes) override {
        received.push_back({kind, bytes});
    }
    void onRouteChanged(NodeAddress d, NodeAddress n, bool added) override {
        routes.push_back({d, n, added});
    }
    void onDataDecision(NodeAddress d, RouteAction a) override { decisions.emplace_back(d, a); }
    void onStateSize(std::uint32_t e, std::uint32_t b) override { states.emplace_back(e, b); }
    std::size_t count(NodeAddress d, NodeAddress n, bool added) const {
        return static_cast<std::size_t>(
            std::count(routes.begin(), routes.end(), Route{d, n, added}));
    }
};

}  // namespace

void TestObserverCallbacks(Checker& c) {
    Recorder rec;
    Harness h(0);
    h.router.setObserver(&rec);

    // The kind table names every type byte.
    for (auto t : {PacketType::Data, PacketType::Hello, PacketType::Seed, PacketType::RouteRequest,
                   PacketType::RouteReply})
        c.check(swarm::core::findKindName(kPacketKindNames, static_cast<std::uint8_t>(t)) !=
                    nullptr,
                "every packet type has a kind name");

    c.check(h.router.originate(9).action == RouteAction::Queue, "unknown destination queues");
    c.check(h.router.originate(0).action == RouteAction::Deliver, "to itself delivers");
    c.check(rec.decisions.size() == 2 &&
                rec.decisions[0] == std::pair{NodeAddress{9}, RouteAction::Queue} &&
                rec.decisions[1] == std::pair{NodeAddress{0}, RouteAction::Deliver},
            "originate reports its decisions, by destination");

    // A data packet deposits (cells appear, the footprint is reported) and
    // is forwarded; its header is data overhead, not control.
    h.router.onReceive(dataFrame(9, 0, 1, 0, 1));  // delivered here
    h.router.onReceive(dataFrame(2, 9, 2, 0, 2));  // forwarded toward 9
    c.check(rec.received.empty() && rec.sent.empty(), "data headers are not control");
    c.check(rec.decisions.size() == 4 && rec.decisions[2].second == RouteAction::Deliver &&
                rec.decisions[3] == std::pair{NodeAddress{9}, RouteAction::Unicast},
            "deliver and forward are reported");
    c.check(!rec.states.empty() &&
                rec.states.back().first == static_cast<std::uint32_t>(h.router.table().size()) &&
                rec.states.back().second == rec.states.back().first * kStateBytesPerEntry,
            "the footprint is the table's entries and bytes");

    // Drop: TTL 0 in transit; an overheard frame reports no data decision.
    const std::size_t before = rec.decisions.size();
    h.router.onReceive(dataFrame(2, 9, 2, 0, 3, 0));
    c.check(rec.decisions.size() == before + 1 && rec.decisions.back().second == RouteAction::Drop,
            "a drop is reported");
    h.router.onReceive(dataFrame(5, 9, 1, 4, 40));  // overheard
    c.check(rec.decisions.size() == before + 1, "an overheard frame is not a decision");

    // Control packets are reported received, with the encoded size.
    PacketHeader hello = dataFrame(3, 3, 3, 0, 7);
    hello.type = PacketType::Hello;
    h.router.onReceive(hello);
    hello.type = PacketType::RouteRequest;
    hello.messageId = 8;
    h.router.onReceive(hello);
    c.check(rec.received.size() == 2 && rec.received[0].kind == 2 && rec.received[1].kind == 4 &&
                rec.received[0].bytes == kPacketWireSize,
            "control receptions carry the kind and the encoded size");
    c.check(rec.sent.empty(), "this slice sends no control packet");
    c.check(rec.decisions.size() == before + 1, "a control packet is not a data decision");
}

void TestObserverRouteChanges(Checker& c) {
    Recorder rec;
    Harness h(0);
    h.router.setObserver(&rec);
    h.router.onReceive(dataFrame(9, 0, 1, 0, 1));
    // New neighbour 1: cell (1,1) then the deposit's column 9: cell (9,1).
    c.check(rec.count(1, 1, true) == 1 && rec.count(9, 1, true) == 1,
            "the entries a first packet creates are reported as added");
    const std::size_t adds = rec.routes.size();
    h.router.onReceive(dataFrame(9, 0, 1, 0, 2));
    c.check(rec.routes.size() == adds, "a deposit on an existing cell is no route change");
    h.router.onReceive(dataFrame(2, 9, 2, 0, 3));
    c.check(rec.count(9, 2, true) == 1 && rec.count(2, 1, true) == 1, "a new neighbour adds a row");

    h.router.onLinkLost(1);
    c.check(rec.count(9, 1, false) == 1 && rec.count(1, 1, false) == 1 &&
                rec.count(2, 1, false) == 1,
            "a lost link removes every cell through it, each reported");
    c.check(rec.states.back().first == static_cast<std::uint32_t>(h.router.table().size()),
            "and the footprint follows");

    // Decay below the floor removes entries, reported removed.
    Recorder dec;
    Harness d(0);
    d.router.setObserver(&dec);
    d.router.onReceive(dataFrame(2, 0, 2, 0, 1));
    d.router.start();
    d.events.runUntil(60.0);
    c.check(d.router.table().empty(), "everything decays away");
    std::size_t adds2 = 0, removes2 = 0;
    for (const auto& r : dec.routes)
        (r.added ? adds2 : removes2)++;
    c.check(adds2 > 0 && adds2 == removes2, "every added entry is removed: adds equal removes");
    c.check(dec.states.back().first == 0 && dec.states.back().second == 0,
            "the empty table is reported");
}

void TestObserverIsReadOnly(Checker& c) {
    Recorder rec;
    const auto with = RunObservedGrid(5, &rec);
    const auto without = RunObservedGrid(5, nullptr);
    c.check(!with.empty() && with == without, "same seed: identical trace with and without");
    c.check(!rec.decisions.empty() && !rec.routes.empty() && !rec.states.empty(),
            "and the observer did see the run");
    // A second, differently seeded run still differs: the check has power.
    c.check(RunObservedGrid(6, nullptr) != without, "another seed: another trace");
}

}  // namespace termite::core::test
