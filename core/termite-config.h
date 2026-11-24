// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// Termite's constants, the ch. 4 version of Roth's dissertation ([R05],
// Table 4.1, p. 84). Every default names its docs/fidelity.md Parameters row:
// a dissertation source, or `repo choice`, or `unknown` with the value this
// core uses until it is decided. Written fresh (golden rules 11 and 13).
#ifndef TERMITE_CORE_CONFIG_H
#define TERMITE_CORE_CONFIG_H

#include "swarm-types.h"

#include <cstddef>
#include <cstdint>

namespace termite::core {

using swarm::core::Time;

struct Config {
    /// Fidelity "ch. 4: initial pheromone": [R05] Table 4.1, p. 84. Every cell
    /// of a new destination column, and of a new neighbour's row and column.
    double initialPheromone = 1.0;
    /// Fidelity "ch. 4: pheromone ceiling": [R05] Table 4.1, p. 84.
    double pheromoneCeiling = 10000.0;
    /// Fidelity "ch. 4: pheromone floor": [R05] Table 4.1, p. 84. Also the
    /// "minimum value" a column or row must reach to be removed (reading).
    double pheromoneFloor = 0.1;
    /// Fidelity "ch. 4: decay rate, τ": [R05] Table 4.1, p. 84; every cell is
    /// multiplied by e^(-τ) once per decay period (eq. 4.2).
    double decayRate = 0.105;
    /// Fidelity "ch. 4: decay period": [R05] Table 4.1, p. 84 (seconds).
    Time decayPeriod = 1.0;
    /// Fidelity "ch. 4: pheromone threshold, K": [R05] §4.2.5, p. 74 (nominal 0).
    double threshold = 0.0;
    /// Fidelity "ch. 4: pheromone sensitivity, F": [RW03] eq. 3, nominal 2
    /// (maintainer decision 2026-11-24, #6). Ch. 4 gives no value (§4.2.5,
    /// p. 74); ch. 7's 10 ([R05] p. 154) is AntHocNet's arm's.
    double sensitivity = 2.0;
    /// Fidelity "ch. 4: pheromone deposit per packet, γc": the constant of
    /// eq. 4.1 (maintainer decision 2026-11-10, #4), nominal 1 in [RW03]
    /// eq. 1 (maintainer decision 2026-11-24, #6), as ch. 5's classic filter.
    double deposit = 1.0;
    /// Fidelity "ch. 4: no next hop although the destination is known": a
    /// data packet whose only neighbour is its arrival link is dropped
    /// ([RW03] §2; maintainer decision 2026-11-24, #6). True sends it back
    /// on that link instead, the bounce of the 2026-11-10 decision (#4): a
    /// default-off mechanism beyond the paper.
    bool deadEndBounce = false;
    /// Fidelity "ch. 4: data TTL": [R05] Table 4.1, p. 84 (hops).
    std::uint8_t dataTtl = 32;
    /// Fidelity "ch. 4: rreq timeout": [R05] Table 4.1, p. 84 (seconds). Data
    /// held for a destination still unknown by then is dropped.
    Time rreqTimeout = 2.0;
    /// Fidelity "ch. 4: RREQs per route request": [R05] Table 4.1, p. 84.
    std::size_t rreqsPerRequest = 2;
    /// Fidelity "ch. 4: RREQ TTL": [R05] Table 4.1, p. 84 (hops).
    std::uint8_t rreqTtl = 32;
    /// Fidelity "ch. 4: RREP TTL": [R05] Table 4.1, p. 84 (hops).
    std::uint8_t rrepTtl = 32;
    /// Fidelity "ch. 4: pending-packet buffer capacity": repo choice (#200),
    /// the arm's shared queue size; org rule 7. Held data, all destinations.
    std::size_t heldCapacity = 64;
    /// Fidelity "ch. 4: duplicate (message-ID) list size": repo choice
    /// (maintainer decision 2026-11-10, #4). It holds forwarded IDs only.
    std::size_t duplicateListSize = 64;
    /// Fidelity "ch. 4: deposited-ID record size": repo choice (#6). One
    /// deposit per packet per node needs the IDs deposited for, overheard ones
    /// included; a node overhears every neighbour (about 13, [R05] p. 90), so
    /// the record is four times the duplicate list. Org rule 7.
    std::size_t depositRecordSize = 256;
    /// Fidelity "ch. 4: pheromone table cap": repo choice (#6), in cells. The
    /// paper sets none (up to N² values, §4.3.3, p. 78); 4096 holds 100
    /// destinations over 40 neighbours. Org rule 7.
    std::size_t tableCapacity = 4096;
};

}  // namespace termite::core

#endif  // TERMITE_CORE_CONFIG_H
