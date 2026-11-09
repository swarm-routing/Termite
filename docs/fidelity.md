# Termite: fidelity sheet

What the paper specifies, what this implementation does, and where they differ.
Every row cites its source (`[KEY] §x`, `Table N`, `Fig. N`, `eq. N`), or says
`repo choice: <reason>`, or `unknown`. Checked by
`swarm-routing/.github` `tools/fidelity/fidelity.py check`.

**State (#4):** only `## Parameters` is written. `## Mechanisms` and
`## Anchor candidate` hold one placeholder row each so the sheet passes the
check; they are the next part of #4.

## Which Termite

Roth's dissertation [R05] describes three versions, with different constants:

| Version | Where | What sets it apart |
|---|---|---|
| ch. 4 Termite | §4.2 (pp. 70–77), Table 4.1 (p. 84) | periodic decay every 1 s, pheromone floor/ceiling/initial value, five packet types (data, hello, seed, RREQ, RREP); RREQs are random walks; forwarding never returns to the arrival link (eq. 4.3) |
| ch. 5 Termite | §5.2 (pp. 93–99), §5.4.1 (pp. 104–105) | no control traffic at all (p. 98); true continuous decay; a cumulative path metric piggybacked on every packet; source pheromone repel R (eq. 5.2); no duplicate detection (p. 99); eleven pheromone update rules compared (§5.3) |
| ch. 7 ReTermite | §7.2 (pp. 144–153), §7.3.1 (pp. 153–154) | ch. 5's forwarding and accounting (γPF, γ̄PF or pBF) plus RREQ flooding and RREP (§7.2.4); the only version compared with AODV |

AntHocNet's `termite` arm followed ch. 4 with Table 4.1, but took F (and,
in effect, its γ) from outside ch. 4, and its anchor came from ch. 7's AODV
comparison. Which version this repository implements is not chosen yet: every
row below is prefixed with its chapter, and the choice goes to the maintainer
(pre-registered in an issue before `core/`, AGENTS.md rule 12). Rows prefixed
`scenario` are the simulation setups, kept for the anchor; they are not
protocol constants.

Read for this sheet: §4.2–§4.4.1, §5.1–§5.4.1 and §5.4.5, §6.3.2 (p. 129),
§7.2–§7.3.1, §7.4.1, and defence slide 15 [R05s]. The GLOBECOM 2003 paper and
the SIDM 2005 chapter were not read.

## Sources

| Key | Citation | Access |
|---|---|---|
| R05 | M. H. Roth, Termite: A Swarm Intelligent Routing Algorithm for Mobile Wireless Ad-Hoc Networks, PhD dissertation, Cornell University, January 2005 (209 pp.; eCommons) | in hand |
| R05s | M. H. Roth, Termite dissertation-defence slides, Cornell University (44 slides). Secondary: superseded by [R05] | in hand |

## Mechanisms

| Mechanism | Source | Implementation | Status |
|---|---|---|---|
| Not yet written (#4, next step) | unknown | — | unknown |

## Parameters

| Parameter | Value | Source | Notes |
|---|---|---|---|
| ch. 4: pheromone deposit per packet, γc | — | unknown | §4.2.2 (eq. 4.1, p. 72) names a constant γc but gives no value, and Table 4.1 omits it. §4.2 (p. 71) also says the deposit depends on path utility, which contradicts a constant: two readings. Ch. 5's classic filter used γc = 1 ([R05] p. 108). The arm used 1 |
| ch. 4: initial pheromone | 1 | [R05] Table 4.1, p. 84 | every cell of a new destination column, and of a new neighbour's row and column (§4.2.4, p. 73–74) |
| ch. 4: pheromone ceiling | 10000 | [R05] Table 4.1, p. 84 | no cell may exceed it (§4.2.4, p. 74) |
| ch. 4: pheromone floor | 0.1 | [R05] Table 4.1, p. 84 | no cell decays below it (§4.2.4, p. 74). Reading: it is also the "minimum value" a column or row must reach to count as decayed and be removed (§4.2.3, p. 73); the paper does not name the two the same |
| ch. 4: decay rate, τ | 0.105 per decay period | [R05] Table 4.1, p. 84 | every cell is multiplied by e^(−τ) ≈ 0.90 once per decay period (eq. 4.2, p. 72). Dimensionless per period here; a rate in s⁻¹ in chs. 5 and 7 |
| ch. 4: decay period | 1 s | [R05] Table 4.1, p. 84 | "nominal" (§4.2.3, p. 72). Periodic, not continuous |
| ch. 4: pheromone threshold, K | 0 | [R05] §4.2.5, p. 74 | "nominal value of K is zero". §6.3.2 ([R05] p. 129) argues Termite must have K > 0 so lesser links keep some probability; with the floor at 0.1 no cell is ever 0, so K = 0 does not zero a link in ch. 4 |
| ch. 4: pheromone sensitivity, F | — | unknown | §4.2.5 (p. 74) gives only its effect (F > 1 sharpens, F = 1 normalises). Ch. 5 finds no gain above F = 10 ([R05] §5.4.5, p. 114); ch. 7 runs F = 10 ([R05] p. 154). The arm used 10 |
| ch. 4: rreq timeout | 2 s | [R05] Table 4.1, p. 84 | a buffered data packet with no reply by then is dropped (§4.2.6, p. 75) |
| ch. 4: RREQs per route request | 2 | [R05] Table 4.1, p. 84 | "may be tuned" (§4.2.6, p. 76) |
| ch. 4: RREQ retries after the timeout | 0 | [R05] §4.2.6, p. 75 | reading: the text drops the packet at the timeout and describes no re-send |
| ch. 4: data TTL | 32 hops | [R05] Table 4.1, p. 84 | the arm uses the harness's IP TTL (64) |
| ch. 4: RREQ TTL | 32 hops | [R05] Table 4.1, p. 84 | |
| ch. 4: RREP TTL | 32 hops | [R05] Table 4.1, p. 84 | |
| ch. 4: seed TTL | 4 hops | [R05] Table 4.1, p. 84 | random walk (§4.2.6, p. 77) |
| ch. 4: seed period | 30 s | [R05] Table 4.1, p. 84 | per node, presumably; not said |
| ch. 4: seeds per period | — | unknown | §4.2.6 (p. 77) gives none |
| ch. 4: seed start phase | — | unknown | not given; the arm draws it uniformly in [0, 30 s) |
| ch. 4: hello period | 1 s | [R05] Table 4.1, p. 84 | hellos only while the node's table is empty, until a reply (§4.2.6, p. 77) |
| ch. 4: hello reply delay or jitter | — | unknown | every hearer replies (p. 77); no delay given. The arm uses U[0, 10 ms) |
| ch. 4: pending-packet buffer capacity | — | unknown | §4.2.6 (p. 75) buffers but sets no cap; org rule 7 needs one |
| ch. 4: duplicate (message-ID) list size and lifetime | — | unknown | the list of forwarded and overheard packets (§4.3.2, p. 78; §4.3.9, p. 82) has no stated cap; org rule 7 needs one |
| ch. 4: pheromone table cap | — | unknown | up to N² values (§4.3.3, p. 78); no cap given |
| ch. 4: neighbour-loss detection (silence time or retries) | — | unknown | a neighbour lost "by means of communications failure" is removed (§4.2.3, p. 73); the paper's perfect MAC never needed a constant |
| ch. 4: header field widths | — | unknown | §4.2.6 (p. 75) lists the six fields (source, destination, previous hop, next hop, message ID, TTL) without sizes |
| ch. 5: pheromone deposit, γc (classic filter) | 1 | [R05] p. 108 | stated in the 50-node results discussion; γPF instead deposits the path utility γ (eq. 5.4, p. 101) |
| ch. 5: path metric (link cost) | distance² | [R05] §5.4.1, p. 105 | "a simple energy model" |
| ch. 5: utility as a function of cost | — | unknown | not written out. K50 = 5×10⁻⁵ = 1/(100² + 100²) suggests utility = 1/(path cost): an inference |
| ch. 5: pheromone threshold, K | K50 = 5×10⁻⁵ (50 nodes), K100 = 2.51×10⁻⁵ (100 nodes) | [R05] §5.4.1, p. 105 | "the utility of a hypothetical link spanning the diagonal of the simulation area": derived from the scenario, not a constant |
| ch. 5: pheromone sensitivity, F | swept 1, 2, 5, 10, 20, 50 | [R05] §5.4.1, p. 105 | no default; no gain above F = 10 ([R05] §5.4.5, p. 114) |
| ch. 5: decay rate, τ | swept 0.1, 0.2, 0.5, 1, 2, 5 s⁻¹ | [R05] §5.4.1, p. 105 | no default; continuous decay over observation inter-arrival times (eq. 5.3, p. 100) |
| ch. 5: source repel sensitivity, R | — | unknown | defined, R ≥ 0 (eq. 5.2, p. 97); no value in §5.4.1. Ch. 6's analytic example uses 0.5 ([R05] p. 128); ch. 7 uses 0.5 |
| ch. 5: TTL | 32 hops | [R05] §5.4.1, p. 105 | all packets |
| ch. 5: ABC+X exploration probability | 5 % | [R05] §5.4.1, p. 105 | only for the ABC+X variants (§5.3.6) |
| ch. 5: piggybacked metric size | 8 bytes | [R05] §5.2.1, p. 95 | "on many platforms"; 4 bytes in ch. 7 |
| ch. 7: pheromone update method | γPF, γ̄PF or pBF | [R05] §7.2.3, eq. 7.2, eq. 7.3, eq. 7.4, p. 149–150 | all three are run; none is named the default |
| ch. 7: pheromone deposit, γ | — | unknown | the packet's cumulative path utility (p. 149); for hop count the formula is not written out. K = 1/32 at TTL 32 suggests γ = 1/hops: an inference |
| ch. 7: pheromone threshold, K | 1/32 | [R05] §7.3.1, p. 154 | |
| ch. 7: pheromone sensitivity, F | 10.0 | [R05] §7.3.1, p. 154 | |
| ch. 7: decay rate, τ | 2.0 s⁻¹ | [R05] §7.3.1, p. 154 | continuous decay. Held at 2.0 "to make each scenario directly comparable"; §7.4.1 ([R05] p. 166) finds τ ≈ 0.1 best at 1 m/s and τ ≈ 1.0 at 10 m/s (γ̄PF) |
| ch. 7: source repel sensitivity, R | 0.5 | [R05] §7.3.1, p. 154 | eq. 7.1 (p. 146) |
| ch. 7: maximum TTL | 32 hops | [R05] §7.3.1, p. 154 | all packets, RREQ floods included; 1-byte field (p. 153) |
| ch. 7: route request hold time | 1.28 s | [R05] §7.3.1, p. 154 | AODV's ACTIVE_ROUTE_TIMEOUT in that setup; held packets dropped after it (§7.2.4, p. 151) |
| ch. 7: RREQ retries | — | unknown | §7.2.4 (p. 150–151) describes none |
| ch. 7: held-packet buffer capacity | — | unknown | every packet for the sought destination is cached (p. 151); no cap; org rule 7 needs one |
| ch. 7: seen-RREQ list size and lifetime | — | unknown | keyed by source and sequence number (p. 151); no cap given |
| ch. 7: initial pheromone | 0 | [R05] §7.2.1, p. 145 | new neighbour rows and new destination columns start at zero |
| ch. 7: pheromone floor and ceiling | — | unknown | ch. 7 describes none; γ̄PF bounds itself by averaging (p. 149–150) |
| ch. 7: destination-removal threshold | — | unknown | a column is removed when "all of the pheromone on it decays" (p. 145), but exponential decay never reaches zero, so a threshold is needed |
| ch. 7: header size | 24 bytes | [R05] §7.2.6, p. 152 | type 1, source 4, destination 4, previous hop 4, next hop 4, pheromone 4 (p. 152), TTL 1, data length 2 (p. 153) |
| scenario ch. 4: nodes, duration | 100 nodes, 600 s | [R05] §4.4.1, p. 83 | Opnet; only node speed varies |
| scenario ch. 4: area, range, bit rate | 50 × 50 m, 10 m, 1 Mbit/s | [R05] Table 4.1, p. 84 | |
| scenario ch. 4: mobility | random waypoint, 60 s initial pause, then none; all nodes the same speed | [R05] §4.4.1, p. 85 | |
| scenario ch. 4: traffic | each node, 1 packet every 0.5 s, 64 B, to one random destination; every packet acknowledged | [R05] §4.4.1, p. 85 | the acknowledgement keeps the sink's pheromone alive |
| scenario ch. 4: MAC | perfect: no errors or collisions, queues modelled | [R05] §4.4.1, p. 85 | ns-3 has no equivalent |
| scenario ch. 7: nodes, area, radio | 100 nodes, 2200 × 600 m, 802.11b at 2 Mbit/s, 250 m range | [R05] §7.3.1, p. 153 | MAC modified for promiscuous reception and to return failed packets to routing (p. 153–154) |
| scenario ch. 7: mobility, duration | random waypoint, zero pause, uniform speed 1, 5, 10, 20 m/s; 600 s | [R05] §7.3.1, p. 154 | ≥ 2 runs at high speeds, 5 at low |
| scenario ch. 7: traffic | 10 nodes, 512 B, exponential inter-arrival mean 0.5 s, each to a unique partner that acknowledges every packet | [R05] §7.3.1, p. 154 | compared with AODV (RFC 3561 parameters) |

## Differences from AntHocNet's termite arm

| Paper | Arm | Difference |
|---|---|---|
| ch. 4: γc not given | `Gamma` = 1 | a value ch. 4 does not state; matches ch. 5's classic filter (p. 108) |
| ch. 4: F not given | `Sensitivity` = 10 | taken from ch. 7 ReTermite (p. 154), a different version |
| ch. 4: periodic decay, e^(−τ) every 1 s (eq. 4.2) | continuous e^(−(t − t_last)·τ), τ = 0.105 per second | chs. 5/7's continuous decay with ch. 4's constant: equal at whole periods, different in between and in the order of decay and deposit |
| ch. 4: data TTL 32 | the harness's IP TTL, 64 | looping data lives twice as long |
| ch. 4: hello reply delay not given | U[0, 10 ms) | an unrecorded repo choice |
| ch. 4: seed phase not given | uniform in [0, seed period) | an unrecorded repo choice |
| ch. 7 is the AODV comparison (p. 153–154) | anchor taken from §7.3, protocol from ch. 4 | the anchor tests a version the arm does not implement (input to the anchor-miss analysis, AGENTS.md rule 12) |

## Anchor candidate

| Trend | Scenario | Metric | Comparators | Source |
|---|---|---|---|---|
| Not yet written (#4, next step); see the `scenario` rows above | TODO | TODO | TODO | unknown |
