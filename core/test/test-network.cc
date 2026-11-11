// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// Routers end to end on a static radio network, every frame through the
// codec and overheard by the sender's other neighbours; and determinism
// (org rule 5): the same seed gives a byte-identical trace.
#include "termite-core-tests.h"

namespace termite::core::test {

namespace {

/// A 3 x 3 grid, nodes 0..8 row by row, trails towards node 0 laid along
/// four paths; then every node sends to 0 and 0 answers each.
std::vector<std::string> runGrid(std::uint64_t seed, int* delivered, int* ended,
                                 swarm::core::IRouterObserver* observer = nullptr) {
    Config cfg;
    cfg.sensitivity = 1.0;  // a flat F, so the draws matter
    Network net(seed, cfg);
    net.observe(observer);
    for (int r = 0; r < 3; ++r) {
        for (int col = 0; col < 3; ++col) {
            const int n = 3 * r + col;
            if (col < 2) net.addLink(n, n + 1);
            if (r < 2) net.addLink(n, n + 3);
        }
    }
    net.layTrail({0, 1, 2, 5, 8});
    net.layTrail({0, 3, 6, 7, 8});
    net.layTrail({0, 1, 4, 7});
    net.layTrail({0, 3, 4, 5});
    for (int round = 0; round < 3; ++round) {
        for (int n = 1; n < 9; ++n) {
            net.send(n, 0);
            net.events().runUntil(net.events().now() + 0.1);
        }
        for (int n = 1; n < 9; ++n) {
            net.send(0, n);
            net.events().runUntil(net.events().now() + 0.1);
        }
    }
    *delivered = net.delivered();
    *ended = net.delivered() + net.dropped() + net.queued();
    return net.trace();
}

}  // namespace

std::vector<std::string> RunObservedGrid(std::uint64_t seed, swarm::core::IRouterObserver* obs) {
    int delivered = 0, ended = 0;
    return runGrid(seed, &delivered, &ended, obs);
}

void TestDeliveryOnALine(Checker& c) {
    Network net(1);
    for (int n = 1; n < 5; ++n)
        net.addLink(n, n + 1);
    net.layTrail({1, 2, 3, 4, 5});
    net.send(5, 1);
    net.events().runUntil(1.0);
    c.check(net.delivered() == 1, "5 to 1 along the trail, overhearing on every hop");
    c.check(net.dropped() == 0, "no overheard copy is taken for a duplicate");
    net.send(1, 5);
    net.events().runUntil(2.0);
    c.check(net.delivered() == 2, "the data laid the trail back: 1 to 5");
}

void TestDeterminism(Checker& c) {
    int delivered = 0, ended = 0;
    const auto other = runGrid(8, &delivered, &ended);
    const auto b = runGrid(7, &delivered, &ended);
    const auto a = runGrid(7, &delivered, &ended);  // its counts are checked below
    c.check(!a.empty() && a == b, "same seed: byte-identical trace");
    c.check(a != other, "another seed: another trace");
    // Without route requests (the next slice) a node off the laid trails
    // queues; a loop meets the duplicate list. Every packet ends once.
    c.check(ended == 48, "each of the 48 packets is delivered, dropped or queued once");
    c.check(delivered > 24, "most are delivered");
}

}  // namespace termite::core::test
