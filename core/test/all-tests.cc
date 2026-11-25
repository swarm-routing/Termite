// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// The list every runner walks.
#include "termite-core-tests.h"

#include <array>

namespace termite::core::test {

namespace {

constexpr std::array<NamedTest, 26> kTests{{
    {"header codec: roundtrip", TestPacketRoundtrip},
    {"header codec: the decoder fails closed", TestPacketDecoderFailsClosed},
    {"header codec: seeded fuzz", TestPacketFuzz},
    {"deposit: eq. 4.1, new rows and columns, ceiling", TestDepositOnReception},
    {"promiscuous: an overheard packet deposits once", TestPromiscuousDepositOnce},
    {"duplicate list: forwarded IDs are dropped untouched", TestDuplicateListDrops},
    {"forwarding: eq. 4.3, never the arrival link", TestForwardingDistribution},
    {"dead end: dropped, as the 2003 paper", TestDeadEndDrops},
    {"dead end, bounce on: back on the arrival link", TestDeadEndBounce},
    {"dead end, bounce on: the bounce is exempt and lowers P(B, d)", TestBounceIsExempt},
    {"unknown destination: queued with its header", TestUnknownDestinationQueues},
    {"route request: two walks, one request per destination", TestRouteRequestOrigination},
    {"route request: uniform walk, never back, duplicates end it", TestRouteRequestWalk},
    {"route request: who answers, and the RREP's fields", TestRouteRequestAnswer},
    {"route reply: routed back, releases the held data", TestRouteReplyReleases},
    {"held data: dropped at the rreq timeout, capped", TestHeldTimeoutAndCap},
    {"end to end: discovery on a chain and a diamond", TestDiscoveryEndToEnd},
    {"TTL: decremented per hop, 0 drops in transit", TestTtl},
    {"decay: eq. 4.2 per period, floor, removals", TestPeriodicDecay},
    {"link loss: the row goes at once", TestLinkLost},
    {"bounded: duplicate list, table, deposit record", TestBoundedStructures},
    {"end to end: a line, both ways", TestDeliveryOnALine},
    {"determinism: same seed, same trace", TestDeterminism},
    {"observer: every callback, data header is not control", TestObserverCallbacks},
    {"observer: pheromone entries appear and disappear", TestObserverRouteChanges},
    {"observer: read-only, same run with and without", TestObserverIsReadOnly},
}};

}  // namespace

std::span<const NamedTest> AllTests() {
    return kTests;
}

}  // namespace termite::core::test
