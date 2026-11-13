# CONTEXT.md — orientation and history

Background an agent or a new contributor needs that does not belong in
`AGENTS.md`: domain notes, current state, glossary, open questions. Read the
section a task needs, not the whole file.

## Current state

**Status: v0.1.0 released (2026-11-11, ungated); v0.2.0 in progress** ([`docs/roadmap.md`](docs/roadmap.md); the protocol issue is #6, under epic #3, the org's Phase 5, swarm-routing/.github#51). On `main`:

- `core/` has the data path of the dissertation's ch. 4 (`core/termite-router.{h,cc}`, `core/termite-packet.{h,cc}`): the eq. 4.1 deposit on every packet, promiscuous reception, the forwarded-ID duplicate list, eq. 4.3 forwarding, the dead-end bounce, periodic decay and the versioned header. It reports through swarm-core's router observer ([`docs/observer.md`](docs/observer.md)).
- `ns3/` is the contrib module `termite`: the adapter carries the core's decisions out on swarm-core's `SwarmRoutingBase`, with the header on every data packet, and registers the `termite` arm of ns3-tools' `routing-compare`.
- Missing (#6): route requests and replies, hello, seed and next-hop failure. Until a node can learn a destination it has not heard from, a fresh network delivers nothing, so there is no example and no determinism program yet, and the campaigns of #31 and #59 have nothing to measure.

Where it comes from: before this repository, Termite existed only as AntHocNet's `termite` comparator arm (GPL-2.0-only, `ns3/baselines/adaptive/`). In AntHocNet v2.5.0 it built, passed the delivery smoke (19.6 %) and **missed its anchor** (the trend points the right way; the CIs span zero), so it shipped as an infeasibility verdict. Its fidelity sheet was written from Roth's 2005 dissertation (chapter 4, Table 4.1; anchor from §4.4); the GLOBECOM 2003 paper and the SIDM 2005 copy were never read (swarm-routing/AntHocNet#587), and still are not: #22 reviews [`docs/fidelity.md`](docs/fidelity.md) against them once they are in hand.

## Glossary

Terms as [`docs/fidelity.md`](docs/fidelity.md) uses them, for the ch. 4 version this repository implements; the row named there has the source and the value.

- **Pheromone table**: a node's routing state, entries P(i, d): the pheromone on neighbour i for destination d ([R05] §4.2.1). `core/` keys it (destination, neighbour) on swarm-core's table.
- **Row**: one neighbour's entries, across every known destination. A row whose entries have all decayed to the floor removes the neighbour.
- **Column**: one destination's entries, across every neighbour. A column whose entries have all decayed to the floor is removed, and the destination is unknown again.
- **γc**: the constant a packet deposits (eq. 4.1): a packet from source s arriving from previous hop r adds γc to P(r, s) at every node that receives it. Ch. 4 gives no value; `core/` uses 1 as a placeholder (#6).
- **F**: the pheromone sensitivity, the exponent of eq. 4.3's forwarding draw: F > 1 sharpens the preference for the strongest link, F = 1 only normalises. Ch. 4 gives no value; `core/` uses 10 as a placeholder (#6).
- **K**: the pheromone threshold added to every entry in eq. 4.3 before the exponent, so weaker links keep some probability. Nominal value 0.
- **Floor, ceiling**: the bounds of an entry (0.1 and 10000, Table 4.1); no entry decays below the floor or exceeds the ceiling.
- **Decay**: every entry is multiplied by e^(−τ) once per decay period (eq. 4.2; τ = 0.105, period 1 s).
- **Bounce**: what a node does with a data packet whose destination it knows but for which the only neighbour is the arrival link: it sends the packet back on that link. The node that gets it back sets its entry for that destination through the dead-end neighbour to the floor; the return is exempt from the duplicate list. A repo choice, ch. 4 is silent.
- **Duplicate list**: the capped list of message IDs a node has forwarded; a copy whose ID is on it is dropped without a table update.
- **Seed**: a control packet that walks at random (TTL 4, every 30 s) to advertise its origin, so destinations stay known. Not implemented yet (#6).

## Open questions

- Why did AntHocNet's arm miss its anchor: the arm (fidelity), the scenario (AntHocNet's anchor cell vs §4.4's setup) or power (seeds)? Pre-registered before code (AGENTS.md rule 12) on #5, accepted 2026-11-10; #59 runs the pre-registered cells and applies the decision rule. It waits for an arm that delivers (#6) and for #95.
- The values of γc and F, which ch. 4 does not give (`core/` holds placeholders; #6).
