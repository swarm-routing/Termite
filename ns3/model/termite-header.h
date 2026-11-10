// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
//
// Termite's per-packet header in ns-3: core/termite-packet.h's header as an
// ns3::Header. The bytes are core/'s wire format (version byte, little
// endian, kPacketWireSize bytes; org rule 3), written and read by core/'s
// codec on swarm-core's bounds-checked helpers, so ns-3 and the standalone
// core share one encoder and one decoder. Written fresh for this module.
//
// Termite's routing state rides inside data packets ([R05] §4.2.6): every
// data packet carries this header between its IPv4 header and its transport
// header, on every hop (docs/fidelity.md, "ch. 4: packet fields"). The IPv4
// protocol field keeps naming the transport; the adapter adds the header
// where a packet enters the radio and removes it before local delivery.
//
// A received packet is untrusted input: the adapter reads it with
// ReadPacketHeader, which refuses a wrong version, an unknown type or a short
// payload. TermiteHeader::Deserialize cannot fail in ns-3's API, so it
// records whether the bytes decoded instead.
#ifndef TERMITE_HEADER_H
#define TERMITE_HEADER_H

#include "termite-packet.h"

#include "ns3/header.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"

#include <optional>

namespace ns3 {
namespace termite {

/// Termite's simulator-free core (core/), from inside ns3::termite.
namespace core = ::termite::core;

class TermiteHeader : public Header {
public:
    TermiteHeader() = default;
    explicit TermiteHeader(const core::PacketHeader& h) : m_header(h), m_valid(true) {}

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    /// Reads kPacketWireSize bytes, or what is left if fewer; IsValid() says
    /// whether they decoded.
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    const core::PacketHeader& Get() const { return m_header; }
    bool IsValid() const { return m_valid; }

private:
    core::PacketHeader m_header;
    bool m_valid = false;
};

/// The header a packet's payload starts with, or nullopt when its first
/// kPacketWireSize bytes are not one valid header (core::decodePacket's rules;
/// a shorter payload is refused). The bytes after it are not read.
std::optional<core::PacketHeader> ReadPacketHeader(Ptr<const Packet> p);

/// A copy of `payload` (an IPv4 payload: transport header and data) with `h`
/// in front.
Ptr<Packet> AddPacketHeader(Ptr<const Packet> payload, const core::PacketHeader& h);

/// A copy of `p` without its first kPacketWireSize bytes (the header
/// ReadPacketHeader accepted).
Ptr<Packet> StripPacketHeader(Ptr<const Packet> p);

}  // namespace termite
}  // namespace ns3

#endif  // TERMITE_HEADER_H
