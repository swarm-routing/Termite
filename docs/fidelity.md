# Termite: fidelity sheet

What the paper specifies, what this implementation does, and where they differ.
Every row cites its source (`[KEY] §x`, `Table N`, `Fig. N`, `eq. N`), or says
`repo choice: <why>`, or `unknown`. Checked by
`swarm-routing/.github` `tools/fidelity/fidelity.py check`.

**State (#4):** every section is written. `core/`'s first slice (#6) implements
the data path: the table, eq. 4.1 on every packet, promiscuous reception, decay
and its removals, link loss, eq. 4.3, the dead end, the duplicate list, the TTL
and the header. The ns-3 module `termite` (`ns3/`) carries that slice out on
swarm-core's `SwarmRoutingBase` with no constant of its own: the header rides
on every data packet between the IPv4 and the transport header, frames
addressed to other nodes are read promiscuously, and a data packet for an
unknown destination is dropped at once until the route request exists (rows
"packet fields" and "unknown destination"). For the other rows the
Implementation column of `## Mechanisms` is still the plan; it says, for each mechanism, whether AntHocNet's `termite`
arm implements it.
The four readings the dissertation does not settle (the deposit, who answers
a RREQ, data at a dead end, the duplicate list) were decided by the maintainer
on 2026-11-10 ([#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)); their rows say so. The papers not read are
reviewed in #22. `## Open for #5` lists the facts the
anchor-miss pre-registration (#5) needs; the analysis itself is #5's.

## Which Termite

Roth's dissertation [R05] describes three versions, with different constants:

| Version | Where | What sets it apart |
|---|---|---|
| ch. 4 Termite | §4.2 (pp. 70–77), Table 4.1 (p. 84) | periodic decay every 1 s, pheromone floor/ceiling/initial value, five packet types (data, hello, seed, RREQ, RREP); RREQs are random walks; forwarding never returns to the arrival link (eq. 4.3) |
| ch. 5 Termite | §5.2 (pp. 93–99), §5.4.1 (pp. 104–105) | no control traffic at all (p. 98); true continuous decay; a cumulative path metric piggybacked on every packet; source pheromone repel R (eq. 5.2); no duplicate detection (p. 99); eleven pheromone update rules compared (§5.3) |
| ch. 7 ReTermite | §7.2 (pp. 144–153), §7.3.1 (pp. 153–154) | ch. 5's forwarding and accounting (γPF, γ̄PF or pBF) plus RREQ flooding and RREP (§7.2.4); the only version compared with AODV |

AntHocNet's `termite` arm followed ch. 4 with Table 4.1, but took F (and,
in effect, its γ) from outside ch. 4. Its v2.5.0 anchor was §4.4's speed
trend for the ch. 4 protocol (goodput and path length against speed), and it
missed: the trend pointed the right way but the CIs spanned zero.

**This repository implements ch. 4** (decided in #4, 2026-11-09): the version
Table 4.1 gives constants for, the one the arm implemented, and the one §4.4's
trend measures. Every row below is still prefixed with its chapter; the ch. 5
and ch. 7 rows are reference only (their Status says `not implemented`). Before
`core/`, #5 must explain the anchor miss with evidence (AGENTS.md rule 12: the
arm, the scenario, or power). Rows prefixed `scenario` are the simulation
setups, kept for the anchor; they are not protocol constants.

Read for this sheet: ch. 4 whole (§4.1–§4.5, pp. 70–91), §5.1–§5.5
(pp. 92–117; the results of §5.4.3–§5.4.4 skimmed for the method ranking),
§6.3.2 (p. 129), ch. 7 §7.1–§7.3.4 and §7.4.1 (pp. 144–166), the
bibliography entries [45], [54]–[56], [80], [81], and defence slide 15
[R05s]. The GLOBECOM 2003 paper [RW03] and the SIDM 2005 chapter [RW06] were
not read: neither is in the papers library (their review: #22). Note that the dissertation cites
its own first Termite report as Med-Hoc-Net 2003 ([R05] p. 194, ref. [56]),
not GLOBECOM 2003; whether the two are the same text is not known.

## Sources

| Key | Citation | Access |
|---|---|---|
| R05 | M. H. Roth, Termite: A Swarm Intelligent Routing Algorithm for Mobile Wireless Ad-Hoc Networks, PhD dissertation, Cornell University, January 2005 (209 pp.; eCommons) | in hand |
| R05s | M. H. Roth, Termite dissertation-defence slides, Cornell University (44 slides). Secondary: superseded by [R05] | in hand |
| RW03 | M. Roth, S. Wicker, Termite: ad-hoc networking with stigmergy, IEEE GLOBECOM 2003, vol. 5, pp. 2937–2941, DOI 10.1109/GLOCOM.2003.1258772. Not read: no row rests on it | institution |
| RW06 | M. Roth, S. Wicker, Termite: A Swarm Intelligent Routing Algorithm for Mobile Wireless Ad-Hoc Networks, in Stigmergic Optimization, Studies in Computational Intelligence 31, Springer, 2006, pp. 155–184, DOI 10.1007/978-3-540-34690-6_7 ("SIDM 2005"). Not read: no row rests on it | browser |
| S97 | R. Schoonderwoerd, O. Holland, J. Bruten, L. Rothkrantz, Ant-Based Load Balancing in Telecommunications Networks, Adaptive Behavior 5(2):169–207, 1997, DOI 10.1177/105971239700500203 (ABC). Background only, for ch. 5's ABC+X; [R05] dates it 1996 (p. 193, ref. [45]), Crossref's publisher record 1997. Not read | institution |

## Mechanisms

The Implementation column is this repository's plan, then `Arm:` whether
AntHocNet's `termite` arm (`ns3/baselines/adaptive/model/termite-routing-protocol.{h,cc}`
and the shared `adaptive-routing-base`, `adaptive-tables.h`) does it.

| Mechanism | Source | Implementation | Status |
|---|---|---|---|
| ch. 4: pheromone table, one row per neighbour, one column per known destination, entries P(i, d) | [R05] §4.2.1, Fig. 4.1 | `core/` table keyed (destination, neighbour); `swarm-core` `PheromoneTable` if it fits. Arm: yes (`BoundedPheromoneTable`) | as written |
| ch. 4: pheromone on every packet: a packet from source s arriving from previous hop r adds γc to P(r, s), at every node that receives it | [R05] §4.2.2, eq. 4.1 | Deposit on reception, for all five packet types. Arm: yes, data and every control type | as written (γc's value: the Parameters row) |
| ch. 4: what a packet deposits: a constant γc (eq. 4.1) or an amount set by the utility of the path it has travelled (p. 71) | [R05] §4.2, p. 71, eq. 4.1 | The constant γc of eq. 4.1. The path-utility deposit is ch. 5's (γPF, eq. 5.4), not implemented; it may come later as a default-off mechanism with its own A/B. Arm: the constant, γc = 1 | as written, eq. 4.1 (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)): p. 71 says "Packet pheromone is proportional to the utility of the path", but eq. 4.1 adds a constant, ch. 4 has no utility field in the packet (§4.2.6), and ch. 5 (p. 94) calls ch. 4's description "vague"; the constant is the only deposit ch. 4 specifies |
| ch. 4: promiscuous reception: nodes not addressed also update their table from the packet | [R05] §4.2.2, p. 72, §4.3.9 | Overheard frames deposit, then are dropped. Arm: yes (data through its `PrevHopTag`, control through the IP source or the walk's origin) | as written |
| ch. 4: periodic decay: every cell multiplied by e^(−τ) once per decay period | [R05] §4.2.3, eq. 4.2 | A decay tick through the injected timer port. Arm: no, continuous decay e^(−τ·Δt) evaluated on read | as written (arm: deviation, see Differences) |
| ch. 4: a destination column is removed when all of it has decayed to the minimum value; a neighbour's column only once its row has decayed too; a decayed row removes the neighbour | [R05] §4.2.3, p. 73 | Checked at each decay tick. Arm: yes, every 1 s | as written; reading: the "minimum value" is the floor (Parameters row) |
| ch. 4: a neighbour lost by communications failure has its row removed at once | [R05] §4.2.3, p. 73 | Through an `ILinkState`-style port fed by the adapter. Arm: yes, a MAC retry-limit drop or an unanswered ARP | as written (the detector: Parameters row, unknown) |
| ch. 4: bounds: new cells start at the initial pheromone (a new non-neighbour source adds a column; a new neighbour a row and a column); no cell decays below the floor or exceeds the ceiling | [R05] §4.2.4, pp. 73–74 | Clamp on deposit and on decay. Arm: yes | as written |
| ch. 4: forwarding: next hop drawn from eq. 4.3 over the neighbours except the arrival link r | [R05] §4.2.5, eq. 4.3 | Proportional draw through the injected RNG port. Arm: yes | as written |
| ch. 4: no next hop although the destination is known (the only neighbour is the arrival link, N − r empty) | repo choice (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)): ch. 4 says nothing ([R05] §4.2.5, §4.2.6); §4.2.6 (p. 76) drops a RREQ that cannot be forwarded, and ch. 5 (p. 98) sends data back to the previous hop or drops it | Send the data packet back on the arrival link; the data TTL (32) bounds any loop. The return to the previous hop A is not a second deposit (A deposited when it first received the packet; one deposit per packet per node, duplicate-list row): its effect is a penalty, lowering A's pheromone for the packet's destination d through the dead-end neighbour B, P(B, d). If A is the packet's source it has not deposited, and the return deposits nothing either: its deposit would be for A itself, a column the table never holds (§4.2.1); A records the ID of a packet it originates as forwarded, so the return is the exempt bounce and the same penalty applies (reading). Arm: the packet is held in the shared queue (64 packets, 30 s) until a new neighbour appears; no route request is sent, since the destination is known | deviation: ch. 4 is silent; this is ch. 5's rule (p. 98) applied to ch. 4. The bounced copy returns to a node that has forwarded its ID; that return is exempt from the duplicate list (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6757064677)): the node processes it normally (lowers pheromone towards the dead-end neighbour, forwards to another neighbour, or bounces it further back if none), and the TTL bounds repeated bounces. The cell is P(B, d) (reading of "lowers pheromone towards that neighbour" for the destination it was forwarded to); the amount is not in ch. 4 (eq. 4.1 only adds): `core/` sets P(B, d) to the floor, the least a known cell holds (repo choice, #6; `TestBounceIsExempt`). A neighbour that only overheard the first send forwards the bounced packet when it is chosen as next hop, since the list holds forwarded IDs only ([#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6757156388); duplicate-list row below) |
| ch. 4: unknown destination: the data packet is buffered and a route request issued; no reply within the rreq timeout and the packet is dropped | [R05] §4.2.6, p. 75 | Buffer per destination, dropped at the rreq timeout. Today (#6): the core returns Queue with the header and the ns-3 adapter drops the packet at once, since no route request would release it; the buffer comes with the route-request slice. Arm: yes, but held packets live in the shared queue (64, 30 s): a reply flushes them; at the timeout they are dropped only if the destination is still unknown | as written (buffer cap: Parameters row) |
| ch. 4: route request: RREQs random-walk (uniform, never back to the arrival link, pheromone ignored) until a node with pheromone for the destination is found; one that cannot be forwarded is dropped; several per request | [R05] §4.2.6, p. 76 | As written, RREQs per request from Table 4.1. Arm: yes; also one request per destination at a time (not in the dissertation) | as written |
| ch. 4: which node answers a RREQ: one "containing pheromone" for the destination. With a floor of 0.1, every known column holds pheromone | [R05] §4.2.6, p. 76, §4.2.4 | Only a node with a cell of the destination's column above the floor answers (or the destination itself): a column held at the floor carries no route information. Arm: any node that has the destination's column, or the destination itself; only the node the RREQ was sent to answers, not a node overhearing it | as written, pre-registered reading of ch. 4's intent (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)): "some pheromone" is either any column (it never falls below the floor until removed) or a cell above the floor; this repository takes the second. With τ = 0.105 a cell decays from 1 to the floor in about 22 s and from 2 (one deposit on a fresh cell) in about 29 s; more deposits last longer, and a column goes only when every cell has decayed (for a neighbour, its row too) |
| ch. 4: route reply: its source is the requested destination and its destination the requester, routed by eq. 4.3, so it lays the destination's trail back to the requester | [R05] §4.2.6, p. 76 | As written. Arm: yes; every node on the way flushes held packets for that destination | as written |
| ch. 4: hello: an isolated node (empty table) broadcasts hellos every hello period until a reply; every node hearing a hello replies; hellos stop until the table is empty again | [R05] §4.2.6, p. 77 | As written. Arm: yes; every node starts isolated (first hello in U[0, 100 ms)), replies jittered U[0, 10 ms) | as written (reply delay: Parameters row) |
| ch. 4: seed: a random walk that advertises its origin, so destinations stay known | [R05] §4.2.6, p. 77, Table 4.1 | As written. Arm: yes, every seed period, TTL 4, start phase U[0, 30 s) | as written (seeds per period, phase: Parameters rows) |
| ch. 4: packet fields: source, destination, previous hop, next hop, message ID, TTL; previous and next hop may come from the MAC header | [R05] §4.2.6, p. 75 | One POD, versioned header (org rule 3), carrying previous and next hop itself rather than reading the MAC header. In ns-3 it travels on every data packet between the IPv4 header and the transport header (the IPv4 protocol field still names the transport), is replaced on every hop and removed before local delivery. Arm: control packets carry their own header (type, TTL, sequence, origin, target, hops); data carries no Termite header, only a `PrevHopTag` standing in for the MAC source, and no message ID | as written (widths: Parameters row, unknown) |
| ch. 4: duplicate list: with message IDs, a node keeps the packets it has forwarded or overheard and drops a packet seen again, without updating its table, which prevents counting an overheard packet twice and kills loops | [R05] §4.3.2, p. 78, §4.3.9, pp. 81–82 | On, with a message ID on every packet; the list is capped (org rule 7; size: Parameters row) and records the IDs this node has forwarded (accepted as next hop), not IDs it only overheard ([#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6757156388)). An overheard copy deposits once (one deposit per packet per node) and is never forwarded; the first copy addressed to the node is processed and forwarded even if its ID was overheard, without a second deposit; a copy whose ID the node has forwarded is dropped without updating its table. Exempt: a packet coming back from the neighbour this node just forwarded it to (the dead-end bounce, row above; [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6757064677)). No header change. Arm: no; every received or overheard copy deposits, loops end only at the TTL Arm: no; every received or overheard copy deposits, loops end only at the TTL | deviation: on (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)), but the list holds forwarded IDs only, not overheard ones as §4.3.2 and §4.3.9 describe; §4.3.9's aim (no double counting) is kept by the one-deposit rule ([#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6757156388)). §4.3.2 makes the list optional ("If loops cannot be tolerated"), §4.3.9 says a promiscuous node "should maintain" the list, §4.2.6 calls the ID "not required"; ch. 4 always runs promiscuous, so §4.3.9 applies (arm: deviation, see Differences) |
| ch. 4: TTL: every packet type carries one; it bounds loops and wrong turns | [R05] §4.3.2, p. 78, Table 4.1 | Per-type TTLs from Table 4.1. Arm: control yes; data no, it uses the harness's IP TTL (64) | as written (arm: deviation, see Differences) |
| ch. 4: a packet whose next hop has gone may be resent to a neighbour that can still be reached | [R05] §4.3.4, p. 79; [R05s] p. 15 | The adapter returns the failed packet to `core/`, which removes the row and forwards again. Arm: yes (`MacReinject`) | as written |
| ch. 4: control packets are forwarded with high priority | [R05s] p. 15 | A priority queue for control, if the adapter's MAC allows it. Arm: no, one MAC queue | as written, from the slides only; the dissertation does not say it |
| ch. 4: links are assumed symmetric: a packet heard from a node means packets can go back to it | [R05] §4.3.11, p. 83 | Assumed; no unidirectional-link handling. Arm: the same | as written |
| ch. 5: true continuous decay, evaluated at every observation of a cell (arrival and forwarding) | [R05] §5.2.1, pp. 94–95, eq. 5.3 | None. Arm: its decay is this, applied to ch. 4 | not implemented: ch. 5 only (ch. 4 decays periodically) |
| ch. 5: the cumulative path metric piggybacked on every packet; the deposit is the path utility γ (γPF) | [R05] §5.2.1, p. 95, eq. 5.4 | None. Arm: no | not implemented: ch. 5 only |
| ch. 5: forwarding may return to the previous hop (eq. 5.1), and source pheromone repel R pushes a packet away from its source (eq. 5.2) | [R05] §5.2.2, pp. 96–97, eq. 5.1, eq. 5.2 | None. Arm: no | not implemented: ch. 5 only |
| ch. 5: no control traffic: an unknown destination is forwarded uniformly; no route repair; no loop prevention; no duplicate detection, every copy updates the table | [R05] §5.2.2, pp. 97–99 | None. Arm: no (it is ch. 4) | not implemented: ch. 5 only |
| ch. 5: eleven pheromone update methods (classic filter, γPF, averaging, γ̄PF, pBF, ABC+X ×3, box filter, oracle, random); γPF has the best goodput, γ̄PF the best delivery efficiency | [R05] §5.3, §5.4.6, eq. 5.3, eq. 5.9 | None. ABC+X borrows ABC's forwarding [S97], as [R05] §5.3.6 describes it | not implemented: ch. 5 only |
| ch. 7: table cells start at zero; a neighbour row stays until the link is explicitly lost; a destination column goes when all of it has decayed | [R05] §7.2.1, p. 145 | None | not implemented: ch. 7 only |
| ch. 7: forwarding with source repel over all neighbours | [R05] §7.2.2, eq. 7.1 | None | not implemented: ch. 7 only |
| ch. 7: γPF, γ̄PF or pBF, with continuous decay, a 4-byte piggybacked metric and promiscuous updates (a node not addressed updates and drops) | [R05] §7.2.3, pp. 147–150, eq. 7.2, eq. 7.3, eq. 7.4 | None | not implemented: ch. 7 only |
| ch. 7: route discovery by flooding: only the designated next hop rebroadcasts, any node overhearing with destination pheromone replies, RREPs follow source pheromone, a seen-RREQ list keyed by (source, sequence number); held packets dropped after the hold time | [R05] §7.2.4, pp. 150–151 | None | not implemented: ch. 7 only |
| ch. 7: no route repair, no route error; a packet is dropped only when every neighbour has gone; three packet types (data, RREQ, RREP), no hello or seed | [R05] §7.2.5, p. 151, §7.2.6, pp. 151–153 | None | not implemented: ch. 7 only |

## Parameters

| Parameter | Value | Source | Notes |
|---|---|---|---|
| ch. 4: pheromone deposit per packet, γc | — | unknown | §4.2.2 (eq. 4.1, p. 72) names a constant γc but gives no value, and Table 4.1 omits it. That the deposit is this constant, not p. 71's path utility, is decided (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)); its value is not. Ch. 5's classic filter used γc = 1 ([R05] p. 108). The arm used 1. `core/` uses 1 as a placeholder until the value is decided (#6) |
| ch. 4: initial pheromone | 1 | [R05] Table 4.1, p. 84 | every cell of a new destination column, and of a new neighbour's row and column (§4.2.4, p. 73–74) |
| ch. 4: pheromone ceiling | 10000 | [R05] Table 4.1, p. 84 | no cell may exceed it (§4.2.4, p. 74) |
| ch. 4: pheromone floor | 0.1 | [R05] Table 4.1, p. 84 | no cell decays below it (§4.2.4, p. 74). Reading: it is also the "minimum value" a column or row must reach to count as decayed and be removed (§4.2.3, p. 73); the paper does not name the two the same |
| ch. 4: decay rate, τ | 0.105 per decay period | [R05] Table 4.1, p. 84 | every cell is multiplied by e^(−τ) ≈ 0.90 once per decay period (eq. 4.2, p. 72). Dimensionless per period here; a rate in s⁻¹ in chs. 5 and 7 |
| ch. 4: decay period | 1 s | [R05] Table 4.1, p. 84 | "nominal" (§4.2.3, p. 72). Periodic, not continuous |
| ch. 4: pheromone threshold, K | 0 | [R05] §4.2.5, p. 74 | "nominal value of K is zero". §6.3.2 ([R05] p. 129) argues Termite must have K > 0 so lesser links keep some probability; with the floor at 0.1 no cell is ever 0, so K = 0 does not zero a link in ch. 4 |
| ch. 4: pheromone sensitivity, F | — | unknown | §4.2.5 (p. 74) gives only its effect (F > 1 sharpens, F = 1 normalises). Ch. 5 finds no gain above F = 10 ([R05] §5.4.5, p. 114); ch. 7 runs F = 10 ([R05] p. 154). The arm used 10. `core/` uses 10 as a placeholder until the value is decided (#6) |
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
| ch. 4: duplicate (message-ID) list size | 64 entries | repo choice (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)): the paper gives no size and org rule 7 needs a cap | the IDs this node has forwarded ([#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6757156388)); §4.3.2 (p. 78) and §4.3.9 (p. 82) also list overheard ones |
| ch. 4: deposited-ID record size | 256 entries | repo choice (#6): four times the duplicate list, since a node overhears every neighbour (about 13, [R05] p. 90) | one deposit per packet per node ([#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6757156388)) needs the IDs a node has deposited for, overheard ones included; bounded by org rule 7 |
| ch. 4: duplicate (message-ID) list entry lifetime | — | unknown | not given, and not decided on 2026-11-10; with the 64-entry cap an entry leaves at the latest when evicted |
| ch. 4: pheromone table cap | 4096 cells | repo choice (#6): the paper gives none and org rule 7 needs one; 100 destinations over 40 neighbours | up to N² values (§4.3.3, p. 78). A new neighbour or destination the table cannot hold is not learned |
| ch. 4: neighbour-loss detection (silence time or retries) | — | unknown | a neighbour lost "by means of communications failure" is removed (§4.2.3, p. 73); the paper's perfect MAC never needed a constant |
| ch. 4: header field widths | 23 bytes: version 1, type 1, source 4, destination 4, previous hop 4, next hop 4, message ID 4, TTL 1 | repo choice (#6): org rule 3; addresses as wide as an IPv4 address, a 1-byte TTL as ch. 7's (p. 153) | §4.2.6 (p. 75) lists the six fields (source, destination, previous hop, next hop, message ID, TTL) without sizes; `core/termite-packet.h`, `kPacketWireVersion` 1 |
| ch. 4: concurrent route requests per destination | — | unknown | §4.2.6 (p. 76) does not say whether a second request may start while one is pending; the arm allows one at a time |
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
| scenario ch. 4: node speeds | 0, 1, 2, 5, 10 m/s | [R05] p. 89 | the speeds of Table 4.2; Fig. 4.4's axis runs 0–10 m/s and Figs. 4.2, 4.3 and 4.5 use a log axis up to 10 m/s. Nothing above 10 m/s |
| scenario ch. 4: link state changes per node per second | 1.18, 2.41, 5.88, 11.25 at 1, 2, 5, 10 m/s | [R05] p. 89 | Table 4.2; 0 at 0 m/s; "approximately linear" in speed |
| scenario ch. 4: mean neighbours per node | slightly under 13 | [R05] p. 90 | "simple calculations based on node density"; a random walk's route confidence is then about 0.08 |
| scenario ch. 4: runs per data point | — | unknown | §4.4 states neither the number of runs nor any spread; the figures carry no error bars |
| scenario ch. 7: nodes, area, radio | 100 nodes, 2200 × 600 m, 802.11b at 2 Mbit/s, 250 m range | [R05] §7.3.1, p. 153 | MAC modified for promiscuous reception and to return failed packets to routing (p. 153–154) |
| scenario ch. 7: mobility, duration | random waypoint, zero pause, uniform speed 1, 5, 10, 20 m/s; 600 s | [R05] §7.3.1, p. 154 | ≥ 2 runs at high speeds, 5 at low |
| scenario ch. 7: traffic | 10 nodes, 512 B, exponential inter-arrival mean 0.5 s, each to a unique partner that acknowledges every packet | [R05] §7.3.1, p. 154 | compared with AODV (RFC 3561 parameters) |

## Differences from AntHocNet's termite arm

What the dissertation's ch. 4 says, what the arm (and its v2.5.0 anchor cell)
does, and the expected effect on §4.4's trend. The protocol rows come from
`ns3/baselines/adaptive/model/termite-routing-protocol.{h,cc}`,
`adaptive-routing-base.cc` and `adaptive-tables.h`; the scenario rows from
`docs/benchmarks/v2.5.0-comparators.md`, the fixture
`ns3/tools/fixtures/termite/termite-anchor.flows` and the `##CONFIG##` line of
`results/cells/anchor-termite-s1-v250c.txt` (AntHocNet `main`). An effect
written "expected" is a hypothesis for #5, not a measurement.

| Paper | Arm | Difference |
|---|---|---|
| ch. 4: γc not given (eq. 4.1) | `Gamma` = 1 | a value ch. 4 does not state; matches ch. 5's classic filter (p. 108). Expected effect: sets the scale F acts on (a fresh column's arrival link goes from 1 to 2, against 1 elsewhere); small on its own |
| ch. 4: F not given (eq. 4.3) | `Sensitivity` = 10 | taken from ch. 7 ReTermite (p. 154), a different version. Expected effect: at F = 10 a 2:1 pheromone ratio becomes 1024:1, so forwarding is nearly deterministic on the most reinforced link; under mobility that link is often stale, so a high F lengthens recovery and data walks. Direction on the trend unknown |
| ch. 4: periodic decay, e^(−τ) every 1 s (eq. 4.2) | continuous e^(−(t − t_last)·τ), τ = 0.105 per second | chs. 5/7's continuous decay with ch. 4's constant: equal at whole periods, up to e^(0.105) ≈ 11 % apart in between, and different in the order of decay and deposit. Expected effect: small |
| ch. 4: data TTL 32 | the harness's IP TTL, 64 | looping data lives twice as long. Expected effect: walks of 33 to 64 hops that ch. 4 would drop are delivered and counted, which raises PDR and the mean data hop count at every speed and widens the hop count's spread; both enter the anchor's two metrics |
| ch. 4: a promiscuous node "should maintain" a list of forwarded and overheard message IDs and drop repeats without updating its table (§4.3.9, pp. 81–82) | no message ID, no duplicate list; every received and overheard copy deposits (this repository: on, 64 entries, maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)) | Expected effect: the misleading gradient §4.3.9 describes (a node credits its downstream neighbour with a path to the source that runs back through itself), so data turns back and loops until the TTL. Candidate cause of the arm's long data paths at low speed (6.65 hops at 1 m/s against AODV's 3.11), which raises the baseline the speed trend is measured from |
| ch. 4: no rule for data at a dead end (N − r empty); this repository sends it back on the arrival link (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)) | held in the shared queue up to 30 s or 64 packets | Expected effect: those packets are delivered late or dropped after 30 s instead of bounced at once; a delay effect more than a PDR one |
| ch. 4: unknown destination, data dropped at the rreq timeout (2 s, p. 75) | dropped at 2 s only if the destination is still unknown; otherwise kept in the 64-packet, 30 s queue until a next hop exists | Expected effect: slightly higher PDR and delay than ch. 4 |
| ch. 4: nothing on concurrent route requests | one request per destination at a time | Expected effect: fewer RREQs; small |
| ch. 4: control packets at high priority ([R05s] p. 15) | one MAC queue | Expected effect: RREQs and RREPs wait behind data; mild at the anchor's load |
| ch. 4: hello reply delay not given | U[0, 10 ms); first hello in U[0, 100 ms) | an unrecorded repo choice. Expected effect: none on the trend |
| ch. 4: seed phase not given | uniform in [0, seed period) | an unrecorded repo choice. Expected effect: none on the trend |
| scenario §4.4.1: perfect MAC, no errors or collisions, queues modelled (p. 85) | ns-3 802.11 (`rateManager=constant2`), `propagation=range` at 250 m | Expected effect: collisions and hidden terminals lose data and control, and a lost neighbour is found only at the MAC retry limit. On the anchor's range-propagation cells this loss is small: of the packets offered, 0.90 % end at the MAC at 1 m/s and 4.43 % at 20 m/s, against 46.41 % and 46.54 % expiring on TTL (`##DROPID##` rows of `anchor-termite-s{1,20}-v250c.txt`). The 30 % MAC loss in the v2.5.0 family results is the Nakagami fading grid, not the anchor |
| scenario §4.4.1: every one of the 100 nodes sends 2 packets/s of 64 B to one random destination (p. 85) | 10 pairs (20 of the 100 nodes), 1024 bit/s each way, 64 B packets (fixture) | a fifth of the senders. Expected effect: far less pheromone-laying traffic; 80 nodes are never sources, so most nodes learn a destination only from passing data, replies and seeds (every 30 s, TTL 4), and a column with no new deposit reaches the floor in about 22 to 29 s (cells at 1 or 2), longer with more deposits. Thinner trails mean more random walking at every speed: the stigmergy §4.4 measures is starved |
| scenario §4.4.1: a reply is returned when a packet arrives, simulating an acknowledgement (p. 85) | an independent reply CBR flow at the same rate, starting 0.25 s after the forward flow | Roth's acknowledgement exists only for a delivered packet and leaves the destination at once, along a trail that just worked; the reply flow is sent regardless and routes by its own trail. Expected effect: at high speed the reverse trail tracks delivery less well; PDR is taken over both directions. The arm's README fidelity notes say the anchor uses acknowledged (TCP) traffic; the committed anchor cells ran UDP (`##CONFIG## … transport=udp`) with this reply flow |
| scenario §4.4.1: 60 s stationary start, then zero pause (p. 85) | `pause=0` from t = 0; flows start at 60 s | Roth's tables form on a static topology before nodes move; the anchor's form while moving. Expected effect: a worse start at high speed, which steepens the trend slightly |
| scenario §4.4.1: speeds 0, 1, 2, 5, 10 m/s (Table 4.2), every node at one speed ([R05] §4.4.1, p. 85) | `--speed` 1, 5, 10, 20 m/s; the rule compares 20 with 1 m/s. Each random-waypoint leg draws its speed from U(1, S): the harness's `--speedMin` defaults to 1 (`routing-bench-harness.cc`, `routing-bench-run.cc`), and the `##CONFIG##` line omits it at 1 | not a fixed speed, and not scaled with the 25× field and range (next row). The long-run (steady-state) time-averaged speed of U(a, b) legs is (b − a)/ln(b/a): 1.00, 2.49, 3.91 and 6.34 m/s for the four cells, which is 0.04, 0.10, 0.16 and 0.25 m/s on §4.4's 10 m scale. These are estimates, not measured run averages: plain random waypoint starts outside its stationary distribution, so a 600 s cell averages somewhat faster than the long-run value at 5–20 m/s (the 1 m/s cell is exact). The contrast therefore spanned roughly a 6× speed ratio, not 20×, and all of it sits below §4.4's 1 m/s; §4.4's 1–10 m/s would be 25–250 m/s here. Expected effect: the anchor probes a much slower topology-change regime than §4.4, where the trend may be flat (#5 hypothesis H2a). Pre-registered ([#5](https://github.com/swarm-routing/Termite/issues/5#issuecomment-6757193490), accepted 2026-11-10): run S re-runs the anchor at fixed speeds 25, 50, 125 and 250 m/s (`--speedMin` = `--speed`), seeds 1..25; run P repeats the v2.5.0 1 and 20 m/s command lines at seeds 1..30 on `ns3-dev:3.48-optimized` (not v2.5.0's ns-3.42; the committed rows are not pooled with it) |
| scenario Table 4.1: 50 × 50 m, 10 m range, 1 Mbit/s | 1250 × 1250 m at 250 m (the same 5 × 5 ranges), 802.11 | the geometry is kept: about 12.4 neighbours per node ignoring the border, against [R05] p. 90's "slightly less than thirteen". Expected effect: small on density; speed is not scaled with it (previous row) |
| scenario §4.4: Opnet; runs per point and spread not stated | ns-3.42 optimized, 10 seeds per speed | Expected effect: power. Termite's PDR SD is 7.2 pp at 1 m/s and 3.6 pp at 20 m/s, so a 5 pp fall does not separate from zero at 10 seeds (v2.5.0: −4.91 pp [−10.36, +0.54]) |

## Anchor candidate

| Trend | Scenario | Metric | Comparators | Source |
|---|---|---|---|---|
| As node speed rises, data goodput falls and the mean path length of delivered data rises sharply, while RREQ and RREP path lengths stay short and about constant | 100 nodes, 50 × 50 m, 10 m range, 1 Mbit/s perfect MAC; random waypoint, 60 s stationary start then no pause, all nodes at one speed, 0 to 10 m/s; 600 s; every node sends 64 B every 0.5 s to one random destination, which acknowledges each packet; Table 4.1 constants | data goodput (delivered fraction of data packets); mean hop count of delivered data; mean hop count of successful RREQs and RREPs | none: §4.4 runs Termite alone (AODV only as context). The org can rerun it except for the perfect MAC, which ns-3 does not have; the nearest is range propagation at a light load | [R05] §4.4.1, §4.4.2, Fig. 4.2, Fig. 4.3, Table 4.1 |

§4.4 gives the trend as curves only (Figs. 4.2–4.5); the text states no
goodput or hop-count value, so the paper sets no effect size to plan a seed
count against. The headline comparison with AODV (ReTermite at least 95 %
goodput against AODV at least 90 %, degrading more gracefully with speed,
[R05] §7.3.3, Fig. 7.1, p. 157) belongs to ch. 7's ReTermite, a different
version: it is not this repository's anchor.

## Open for #5

The facts on this sheet that the anchor-miss pre-registration (#5) needs.
None of them is a verdict; #5 decides which hypothesis each supports and which
runs tell them apart.

- **Arm deviations from ch. 4** that act at every speed (see the Differences
  table): no duplicate list despite promiscuous reception (§4.3.9); F = 10 from
  ch. 7; data TTL 64, not 32; continuous, not periodic, decay; dead-end and
  held packets kept up to 30 s; no control priority. The first two and the
  TTL bear directly on the hop-count metric. The missing duplicate list is the
  first suspect (maintainer decision 2026-11-10, [#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)).
  On the anchor cells about 46 % of offered packets expire on TTL at both 1
  and 20 m/s, and under 5 % end at the MAC (`##DROPID##` rows): the loss
  that dominates is looping or wandering data, not the channel.
- **Readings ch. 4 leaves open** that the arm fixed silently: the deposit
  (constant γc or path utility, p. 71 against eq. 4.1); which node may answer
  a RREQ (any known column, or a cell above the floor); data at a dead end.
  Decided for this repository on 2026-11-10 ([#4](https://github.com/swarm-routing/Termite/issues/4#issuecomment-6756868538)): the constant; a
  cell above the floor; send it back on the arrival link. The arm took the
  constant, any known column, and a 30 s hold.
- **Scenario deviations**: an 802.11 MAC instead of a perfect one; 20 of the
  100 nodes sending instead of all 100; a reply flow instead of a
  per-delivery acknowledgement; speeds not scaled with the 25× field and
  drawn per leg from U(1, S), not fixed, so the anchor's long-run time-averaged
  1–6.34 m/s (an estimate) is 0.04–0.25 m/s on §4.4's scale (§4.4 ran 0–10 m/s, every node
  at one speed); no stationary start; ns-3.42 instead of Opnet. The field
  scaling keeps the neighbour count (about 12.4 against "slightly less than
  thirteen").
- **Pre-registration of record** ([#5](https://github.com/swarm-routing/Termite/issues/5#issuecomment-6757193490), accepted by the maintainer
  on 2026-11-10, [acceptance](https://github.com/swarm-routing/Termite/issues/5#issuecomment-6757485387)): hypotheses H1 arm, H2 scenario and H3 power, with their
  evidence from the committed rows; run S (fixed speeds 25, 50, 125 and
  250 m/s, Termite and AODV, seeds 1..25), run P (the v2.5.0 1 and 20 m/s
  command lines, Termite, seeds 1..30, on `ns3-dev:3.48-optimized`, not pooled
  with the committed ns-3.42 rows), stage 2 only on an S miss; the
  rule is a Welch 95 % CI of highest minus lowest speed on both PDR and
  delivered-data hops, with fixed seed counts. RREQ and RREP path lengths
  are reported for this repository's arm (#6) only.
- **Metric match**: §4.4's goodput counts data packets ([R05] p. 86); the
  anchor's PDR is FlowMonitor's over all 20 flows, reply flows included.
  §4.4's path length is over delivered data, as the anchor's `##ROUTE##` hop
  count is.
- **Power**: §4.4 states no runs per point and no spread, and no number for
  either metric, so the expected effect size has to come from the committed
  v2.5.0 rows (`results/cells/anchor-termite-s{1,5,10,20}-v250c.txt`), not from
  the paper. Done in the pre-registration: at the observed effect both gated
  metrics pass with 80 % probability at about 30 seeds per speed (0.37 at the
  10 seeds v2.5.0 ran).
- **Absolute level**: the arm delivered 42–47 % at every speed, against
  AODV's 67–78 % on the same cells. §4.4 gives no number to compare that with;
  ch. 5's 96 % for γPF at 10 m/s ([R05] p. 108) is a different version on a
  perfect MAC with one flow.
