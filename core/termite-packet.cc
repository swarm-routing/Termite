// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// The header codec, on swarm-core's bounds-checked little-endian helpers.
#include "termite-packet.h"

#include "swarm-wire.h"

namespace termite::core {

void encodePacket(const PacketHeader& header, std::vector<std::uint8_t>& out) {
    swarm::core::WireWriter w(out);
    w.version(kPacketWireVersion);
    w.u8(static_cast<std::uint8_t>(header.type));
    w.i32(header.source);
    w.i32(header.destination);
    w.i32(header.previousHop);
    w.i32(header.nextHop);
    w.u32(header.messageId);
    w.u8(header.ttl);
}

std::optional<PacketHeader> decodePacket(std::span<const std::uint8_t> bytes) {
    swarm::core::WireReader r(bytes);
    r.expectVersion(kPacketWireVersion);
    PacketHeader h;
    const std::uint8_t type = r.u8();
    if (type < static_cast<std::uint8_t>(PacketType::Data) ||
        type > static_cast<std::uint8_t>(PacketType::RouteReply)) {
        r.fail();
    }
    h.type = static_cast<PacketType>(type);
    h.source = r.i32();
    h.destination = r.i32();
    h.previousHop = r.i32();
    h.nextHop = r.i32();
    h.messageId = r.u32();
    h.ttl = r.u8();
    if (!r.atEnd()) return std::nullopt;
    return h;
}

}  // namespace termite::core
