# What the core reports to swarm-core's router observer

Part of swarm-routing/Termite#41 (swarm-routing/ns3-swarm-core#42, #44). The
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
`SEED`, `RREQ`, `RREP`), for `SwarmRoutingHelper::SetKindNames` once the ns-3
adapter lands (#37).
