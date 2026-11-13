// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
//
// Termite's ns-3 adapter: an ns3::SwarmRoutingBase that hosts one
// core::Router (core/termite-router.h) per node and carries out its decisions
// (golden rule 10). It only converts: the core's header to and from the bytes
// every data packet carries (TermiteHeader, right after the UDP header, or in
// front of any other transport header), IPv4 addresses to core addresses, and the router's
// RouteDecisions into sends, deliveries and drops. Time, randomness and timers
// are swarm-core's ports over ns-3 (Simulator::Schedule, a stream
// AssignStreams pins), so a fixed seed gives byte-identical runs (org rule 5).
// The protocol constants are core::Config's defaults, each sourced in
// docs/fidelity.md; the adapter has none of its own. Written fresh on
// swarm-core; the UDP port is the one AntHocNet's `termite` arm used.
//
// What the core decides, the adapter does:
//  - data this node originates: SelectNextHop answers no next hop, so
//    SwarmRoutingBase sends the packet through the loopback device to
//    RouteInput, where the transport header is in place; there the core's
//    originate() gives the header to add and the next hop. The IPv4 TTL is
//    given back the decrement that loopback hop cost, as the base does.
//  - data received from a neighbour: the header is read and removed, and
//    core::Router::onReceive decides: Deliver hands the packet without the
//    header to the local transport; Unicast sends it on with the core's new
//    header; Drop drops it through the error callback.
//  - Queue (the destination is unknown): the core's route request is its next
//    slice (#6), so nothing would ever release the packet; it is dropped at
//    once, not held (docs/fidelity.md, "ch. 4: unknown destination").
//  - promiscuous reception ([R05] §4.2.2): every 802.11 frame addressed to
//    another node is read too, and a valid header goes to onReceive, which
//    deposits and returns None.
//  - a link failure (swarm-core's WifiLinkMonitor): core::Router::onLinkLost.
//  - decay: core::Router::start(), on the Timers() port.
#ifndef TERMITE_ROUTING_PROTOCOL_H
#define TERMITE_ROUTING_PROTOCOL_H

#include "ns3/net-device.h"
#include "ns3/swarm-route-decision.h"
#include "ns3/swarm-routing-base.h"
#include "ns3/traced-callback.h"

#include <cstdint>
#include <memory>

namespace termite::core {
class Router;
struct PacketHeader;
}  // namespace termite::core

namespace ns3 {
namespace termite {

/// Termite's simulator-free core (core/), from inside ns3::termite.
namespace core = ::termite::core;

class RoutingProtocol : public SwarmRoutingBase {
public:
    /// UDP port of Termite's control packets (AntHocNet's `termite` arm: 5542).
    /// The core's first slice sends none; hello, seed, RREQ and RREP use it.
    static constexpr uint16_t kTermitePort = 5542;

    static TypeId GetTypeId();
    RoutingProtocol();
    ~RoutingProtocol() override;

    /// The pheromone this node holds for reaching `destination` through
    /// `neighbor`, or 0 when its table has no such cell (or the protocol has
    /// not started).
    double Pheromone(Ipv4Address destination, Ipv4Address neighbor) const;

    /// A data decision this node carried out: the packet's (source, message
    /// ID), the swarm::core::RouteAction as its byte, and the next hop
    /// (Ipv4Address() unless the action is Unicast).
    typedef void (*DataCallback)(Ipv4Address source, uint32_t messageId, uint8_t action,
                                 Ipv4Address nextHop);

    bool RouteInput(Ptr<const Packet> p, const Ipv4Header& header, Ptr<const NetDevice> idev,
                    const UnicastForwardCallback& ucb, const MulticastForwardCallback& mcb,
                    const LocalDeliverCallback& lcb, const ErrorCallback& ecb) override;

protected:
    void DoInitialize() override;
    void DoDispose() override;

    uint16_t ControlPort() const override { return kTermitePort; }
    Ipv4Address SelectNextHop(Ipv4Address dst, Ipv4Address src, Ipv4Address prev,
                              Ptr<const Packet> p) override;
    void StartDiscovery(Ipv4Address dst) override;
    void RecvControl(Ptr<Packet> p, Ipv4Address from, Ipv4Address receiver) override;
    void OnLinkFailure(Ipv4Address neighbor) override;

private:
    /// Carries out the core's decision on a data packet: `payload` is the
    /// IPv4 payload without a Termite header; `in` names the packet for the
    /// Data trace when the decision carries no header.
    void Carry(const swarm::core::RouteDecision<core::PacketHeader>& d,
               const core::PacketHeader& in, Ptr<const Packet> payload, const Ipv4Header& header,
               int32_t iif, const UnicastForwardCallback& ucb, const LocalDeliverCallback& lcb,
               const ErrorCallback& ecb);
    /// Promiscuous receive handler: frames addressed to other nodes.
    void Overhear(Ptr<NetDevice> device, Ptr<const Packet> frame, uint16_t protocol,
                  const Address& from, const Address& to, NetDevice::PacketType type);

    std::unique_ptr<core::Router> m_router;
    TracedCallback<Ipv4Address, uint32_t, uint8_t, Ipv4Address> m_dataTrace;
};

}  // namespace termite
}  // namespace ns3

#endif  // TERMITE_ROUTING_PROTOCOL_H
