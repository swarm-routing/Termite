// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
#include "termite-header.h"

#include "ns3/udp-header.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace ns3 {
namespace termite {

NS_OBJECT_ENSURE_REGISTERED(TermiteHeader);

TypeId TermiteHeader::GetTypeId() {
    static TypeId tid = TypeId("ns3::termite::TermiteHeader")
                            .SetParent<Header>()
                            .SetGroupName("Termite")
                            .AddConstructor<TermiteHeader>();
    return tid;
}

TypeId TermiteHeader::GetInstanceTypeId() const {
    return GetTypeId();
}

uint32_t TermiteHeader::GetSerializedSize() const {
    return static_cast<uint32_t>(core::kPacketWireSize);
}

void TermiteHeader::Serialize(Buffer::Iterator start) const {
    std::vector<uint8_t> bytes;
    core::encodePacket(m_header, bytes);
    start.Write(bytes.data(), static_cast<uint32_t>(bytes.size()));
}

uint32_t TermiteHeader::Deserialize(Buffer::Iterator start) {
    const uint32_t n = std::min(start.GetRemainingSize(), GetSerializedSize());
    std::vector<uint8_t> bytes(n);
    if (n > 0) start.Read(bytes.data(), n);  // data() may be null when empty
    const auto h = core::decodePacket(bytes);
    m_valid = h.has_value();
    m_header = h.value_or(core::PacketHeader{});
    return n;
}

void TermiteHeader::Print(std::ostream& os) const {
    if (!m_valid) {
        os << "invalid termite header";
        return;
    }
    os << "type=" << static_cast<uint32_t>(m_header.type) << " src=" << m_header.source
       << " dst=" << m_header.destination << " prev=" << m_header.previousHop
       << " next=" << m_header.nextHop << " id=" << m_header.messageId
       << " ttl=" << static_cast<uint32_t>(m_header.ttl);
}

namespace {

/// True when the header sits after a UDP header in `p`: a UDP payload long
/// enough to hold one.
bool AfterUdp(Ptr<const Packet> p, uint8_t protocol) {
    return protocol == kUdpProtocol && p->GetSize() >= UdpHeader().GetSerializedSize();
}

}  // namespace

std::optional<core::PacketHeader> ReadPacketHeader(Ptr<const Packet> p, uint8_t protocol) {
    if (!p) return std::nullopt;
    const uint32_t at = protocol == kUdpProtocol ? UdpHeader().GetSerializedSize() : 0;
    if (p->GetSize() < at + core::kPacketWireSize) return std::nullopt;
    std::vector<uint8_t> bytes(at + core::kPacketWireSize);
    p->CopyData(bytes.data(), static_cast<uint32_t>(bytes.size()));
    bytes.erase(bytes.begin(), bytes.begin() + at);
    return core::decodePacket(bytes);
}

Ptr<Packet> AddPacketHeader(Ptr<const Packet> payload, uint8_t protocol,
                            const core::PacketHeader& h) {
    Ptr<Packet> q = payload->Copy();
    UdpHeader udp;
    const bool afterUdp = AfterUdp(q, protocol);
    if (afterUdp) q->RemoveHeader(udp);
    q->AddHeader(TermiteHeader(h));
    if (afterUdp) q->AddHeader(udp);
    return q;
}

Ptr<Packet> StripPacketHeader(Ptr<const Packet> p, uint8_t protocol) {
    Ptr<Packet> q = p->Copy();
    UdpHeader udp;
    const bool afterUdp = AfterUdp(q, protocol);
    if (afterUdp) q->RemoveHeader(udp);
    q->RemoveAtStart(static_cast<uint32_t>(core::kPacketWireSize));
    if (afterUdp) q->AddHeader(udp);
    return q;
}

}  // namespace termite
}  // namespace ns3
