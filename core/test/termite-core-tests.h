// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// Termite's core test functions. Each records its failures in the Checker;
// the runners (run-core-tests.cc, and the ns-3 suite once ns3/ exists)
// report them.
#ifndef TERMITE_CORE_TESTS_H
#define TERMITE_CORE_TESTS_H

#include "termite-test-support.h"

#include <span>

namespace termite::core::test {

void TestPacketRoundtrip(Checker& c);
void TestPacketDecoderFailsClosed(Checker& c);
void TestPacketFuzz(Checker& c);
void TestDepositOnReception(Checker& c);
void TestPromiscuousDepositOnce(Checker& c);
void TestDuplicateListDrops(Checker& c);
void TestForwardingDistribution(Checker& c);
void TestDeadEndBounce(Checker& c);
void TestBounceIsExempt(Checker& c);
void TestUnknownDestinationQueues(Checker& c);
void TestTtl(Checker& c);
void TestPeriodicDecay(Checker& c);
void TestLinkLost(Checker& c);
void TestBoundedStructures(Checker& c);
void TestDeliveryOnALine(Checker& c);
void TestDeterminism(Checker& c);

struct NamedTest {
    const char* name;
    void (*run)(Checker&);
};

/// Every core test, in a fixed order.
std::span<const NamedTest> AllTests();

}  // namespace termite::core::test

#endif  // TERMITE_CORE_TESTS_H
