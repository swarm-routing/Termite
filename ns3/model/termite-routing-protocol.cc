// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
#include "termite-routing-protocol.h"

#include "termite-header.h"
#include "termite-router.h"

#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/loopback-net-device.h"
#include "ns3/node.h"
#include "ns3/swarm-ns3-ports.h"
#include "ns3/udp-header.h"

namespace ns3 {
namespace termite {

NS_LOG_COMPONENT_DEFINE("TermiteRoutingProtocol");
NS_OBJECT_ENSURE_REGISTERED(RoutingProtocol);

namespace {

/// EtherType of IPv4, the frames the promiscuous handler reads. Spelled out:
/// ns-3 deprecates its protocol-number constants between releases.
constexpr uint16_t kIpv4EtherType = 0x0800;
/// IANA protocol number of UDP.
constexpr uint8_t kUdpProtocol = 17;

using swarm::core::RouteAction;

}  // namespace

TypeId RoutingProtocol::GetTypeId() {
    static TypeId tid =
        TypeId("ns3::termite::RoutingProtocol")
            .SetParent<SwarmRoutingBase>()
            .SetGroupName("Termite")
            .AddConstructor<RoutingProtocol>()
            .AddTraceSource("Data",
                            "A data decision this node carried out (source, message ID, "
                            "RouteAction byte, next hop).",
                            MakeTraceSourceAccessor(&RoutingProtocol::m_dataTrace),
                            "ns3::termite::RoutingProtocol::DataCallback");
    return tid;
}

RoutingProtocol::RoutingProtocol() = default;

RoutingProtocol::~RoutingProtocol() = default;

void RoutingProtocol::DoInitialize() {
    // The interfaces are up by now (addresses are assigned before the
    // simulation starts), so the node's address is known. The protocol
    // constants are core::Config's defaults, each sourced in docs/fidelity.md.
    m_router = std::make_unique<core::Router>(ToNodeAddress(MainAddress()), core::Config{}, Rng(),
                                              Timers());
    m_router->start();
    // Promiscuous reception ([R05] §4.2.2): frames addressed to other nodes,
    // on every device but the loopback.
    if (Ptr<Node> node = GetObject<Node>()) {
        for (uint32_t i = 0; i < node->GetNDevices(); ++i) {
            Ptr<NetDevice> dev = node->GetDevice(i);
            if (DynamicCast<LoopbackNetDevice>(dev)) continue;
            node->RegisterProtocolHandler(MakeCallback(&RoutingProtocol::Overhear, this),
                                          kIpv4EtherType, dev, true);
        }
    }
    SwarmRoutingBase::DoInitialize();
}

void RoutingProtocol::DoDispose() {
    // The base cancels the core's pending timers first; then the router goes.
    SwarmRoutingBase::DoDispose();
    m_router.reset();
}

double RoutingProtocol::Pheromone(Ipv4Address destination, Ipv4Address neighbor) const {
    if (!m_router) return 0.0;
    const auto d = ToNodeAddress(destination);
    const auto n = ToNodeAddress(neighbor);
    return m_router->table().contains(d, n) ? m_router->table().get(d, n) : 0.0;
}

Ipv4Address RoutingProtocol::SelectNextHop(Ipv4Address /*dst*/, Ipv4Address /*src*/,
                                           Ipv4Address /*prev*/, Ptr<const Packet> /*p*/) {
    // No next hop in RouteOutput: the transport header is added after it, and
    // the Termite header goes in front of the transport header. The base sends
    // the packet through the loopback device to RouteInput, which originates it.
    return Ipv4Address();
}

void RoutingProtocol::StartDiscovery(Ipv4Address dst) {
    // RouteInput handles every data packet, so the base never holds one; if it
    // ever did, the core has no route request to release it yet (#6).
    DiscardDeferred(dst);
}

void RoutingProtocol::RecvControl(Ptr<Packet> /*p*/, Ipv4Address from, Ipv4Address /*receiver*/) {
    // The core's first slice sends no control packets (hello, seed, RREQ and
    // RREP are its next slices, #6).
    NS_LOG_DEBUG("ignored a control packet from " << from);
}

void RoutingProtocol::OnLinkFailure(Ipv4Address neighbor) {
    if (m_router) m_router->onLinkLost(ToNodeAddress(neighbor));
}

bool RoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header& header,
                                 Ptr<const NetDevice> idev, const UnicastForwardCallback& ucb,
                                 const MulticastForwardCallback& mcb,
                                 const LocalDeliverCallback& lcb, const ErrorCallback& ecb) {
    const Ipv4Address dst = header.GetDestination();
    Ptr<Ipv4> ipv4 = idev ? idev->GetNode()->GetObject<Ipv4>() : nullptr;
    bool control = false;
    if (header.GetProtocol() == kUdpProtocol) {
        UdpHeader udp;
        control = p->PeekHeader(udp) && udp.GetDestinationPort() == kTermitePort;
    }
    // Control packets, broadcast and multicast are the base's.
    if (!m_router || !ipv4 || control || dst.IsBroadcast() || dst.IsMulticast()) {
        return SwarmRoutingBase::RouteInput(p, header, idev, ucb, mcb, lcb, ecb);
    }
    const int32_t iif = ipv4->GetInterfaceForDevice(idev);
    if (DynamicCast<const LoopbackNetDevice>(idev)) {
        if (ipv4->IsDestinationAddress(dst, iif)) {
            return SwarmRoutingBase::RouteInput(p, header, idev, ucb, mcb, lcb, ecb);
        }
        // This node's own data, sent here by RouteOutput. It leaves through
        // ucb -> IpForward, which decrements the TTL for a hop no radio
        // carried: add it back (as SwarmRoutingBase does for its loopback).
        Ipv4Header own = header;
        own.SetTtl(header.GetTtl() + 1);
        const auto d = m_router->originate(ToNodeAddress(dst));
        const core::PacketHeader in{core::PacketType::Data, ToNodeAddress(MainAddress())};
        Carry(d, d.message ? *d.message : in, p, own, iif, ucb, lcb, ecb);
        return true;
    }
    // From a neighbour: the payload starts with the Termite header.
    const auto h = ReadPacketHeader(p);
    if (!h || h->destination != ToNodeAddress(dst)) {
        NS_LOG_DEBUG("no valid Termite header on a data packet for " << dst);
        return false;
    }
    Carry(m_router->onReceive(*h), *h, StripPacketHeader(p), header, iif, ucb, lcb, ecb);
    return true;
}

void RoutingProtocol::Carry(const swarm::core::RouteDecision<core::PacketHeader>& d,
                            const core::PacketHeader& in, Ptr<const Packet> payload,
                            const Ipv4Header& header, int32_t iif,
                            const UnicastForwardCallback& ucb, const LocalDeliverCallback& lcb,
                            const ErrorCallback& ecb) {
    const Ipv4Address next =
        d.action == RouteAction::Unicast ? ToIpv4Address(d.nextHop) : Ipv4Address();
    m_dataTrace(ToIpv4Address(in.source), in.messageId, static_cast<uint8_t>(d.action), next);
    Ipv4Header out = header;
    switch (d.action) {
    case RouteAction::Unicast: {
        // The core's header for this hop, in front of the transport header.
        Ptr<Packet> q = AddPacketHeader(payload, *d.message);
        out.SetPayloadSize(q->GetSize());
        ForwardData(q, out, next, ucb);
        return;
    }
    case RouteAction::Deliver:
        out.SetPayloadSize(payload->GetSize());
        if (!lcb.IsNull()) lcb(payload, out, static_cast<uint32_t>(iif));
        return;
    case RouteAction::None:
        // A frame the core only learns from (not addressed data): nothing to send.
        return;
    default:
        // Drop, and Queue: the core's route request (which would release a
        // held packet) is its next slice, so the packet is dropped now.
        out.SetPayloadSize(payload->GetSize());
        if (!ecb.IsNull()) ecb(payload, out, Socket::ERROR_NOROUTETOHOST);
        return;
    }
}

void RoutingProtocol::Overhear(Ptr<NetDevice> /*device*/, Ptr<const Packet> frame,
                               uint16_t /*protocol*/, const Address& /*from*/,
                               const Address& /*to*/, NetDevice::PacketType type) {
    // Only frames addressed to another node: the rest arrive through RouteInput.
    if (type != NetDevice::PACKET_OTHERHOST || !m_router) return;
    Ptr<Packet> q = frame->Copy();
    Ipv4Header ip;
    if (q->GetSize() < ip.GetSerializedSize()) return;
    q->RemoveHeader(ip);
    if (ip.GetFragmentOffset() != 0) return;  // only a first fragment starts with the header
    const auto h = ReadPacketHeader(q);
    if (!h || h->nextHop == m_router->self()) return;
    m_router->onReceive(*h);  // deposits once; an overheard frame is never forwarded
}

}  // namespace termite
}  // namespace ns3
