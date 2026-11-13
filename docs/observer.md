# What the core reports to swarm-core's router observer

Added by swarm-routing/Termite#41 (closed 2026-11-11; the swarm-core side is
swarm-routing/ns3-swarm-core#42 and swarm-routing/ns3-swarm-core#44). The
`Router` takes an optional `swarm::core::IRouterObserver*`
(`Router::setObserver`; null, the default, is one pointer check per event). An
observer only reads: no decision, random draw or table change depends on it
(org rule 5; `TestObserverIsReadOnly` runs the same seed with and without one
and compares the traces).

## Events

| Callback | When | Notes |
|----------|------|-------|
| `onControlReceived(kind, bytes)` | `onReceive` is handed a non-data packet (hello, seed, RREQ, RREP), addressed or overheard | `kind` is the wire type byte, `bytes` is `kPacketWireSize` (23) |
| `onControlSent(kind, broadcast, bytes)` | never yet | Termite has no control ants; this slice sends no hello, seed, RREQ or RREP. It is reported where the next slice sends them |
| `onRouteChanged(dest, nextHop, added)` | a pheromone cell `(dest, neighbour)` appears (new neighbour's row, new column, first deposit) or disappears (decay to the floor, lost link) | a deposit on an existing cell, and decay of a live cell, are not route changes |
| `onDataDecision(dest, action)` | each data packet originated or received: Unicast (forwarded), Queue, Drop, Deliver | an overheard frame (`None`) and control packets are not data decisions |
| `onStateSize(entries, bytes)` | after `onReceive`, `decay` and `onLinkLost`, when the cell count changed | `bytes = entries * kStateBytesPerEntry` (16: two addresses and a double) |

## How the pheromone is counted

Termite's state rides inside **data** packets: the header of a data packet
(`kPacketWireSize` bytes) is data overhead, never reported as control, so a
Termite run shows no control traffic except the hello/seed/RREQ/RREP packets
of later slices. The cost readers compare to other arms is therefore the data
header's constant size, known from `kPacketWireSize`, not a counter.

## Kind names

`kPacketKindNames` (`termite-packet.h`) names the type bytes (`DATA`, `HELLO`,
`SEED`, `RREQ`, `RREP`). The ns-3 adapter registers the table itself (next
section), so a scenario does not call `SwarmRoutingHelper::SetKindNames` for
Termite; a table set through it later replaces this one.

## The ns-3 half

The adapter (`ns3/model/termite-routing-protocol.cc`) does two things and adds
no observer code of its own:

- `RoutingProtocol::GetTypeId()` registers the kind names once, with the
  TypeId: `SwarmRoutingBase::SetKindNames(tid, core::kPacketKindNames)`.
- `DoInitialize()` hands the core swarm-core's observer:
  `m_router->setObserver(&Observer())`.

From there the events above reach swarm-core's `SwarmRoutingBase`
(`model/swarm-routing-base.h` in swarm-routing/ns3-swarm-core): its trace
sources `ControlTx`, `ControlRx`, `RouteChanged`, `DataDecision` and
`StateSize`, the per-node counters of `GetRouterStats()`, and
`SwarmRoutingHelper::CollectStats` over a set of nodes.

`ObserverTestCase` in `ns3/test/termite-test-suite.cc` (`test.py -s termite`)
covers it on a four-node diamond: `RouteChanged`, `DataDecision` and
`StateSize` fire, no control event fires (a data header is not control),
`GetKindName(GetTypeId(), 4)` is `RREQ`, and the run with the sources
connected has the same trace and the same delivery as the run without them.
