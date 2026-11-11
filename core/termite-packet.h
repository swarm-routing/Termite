// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// The Termite header every packet carries, and its wire format. Written fresh
// from [R05] §4.2.6 (p. 75) and docs/fidelity.md (golden rule 13).
//
// [R05] §4.2.6 lists six fields: source, destination, previous hop, next hop,
// message ID and TTL, and five packet types (data, hello, seed, RREQ, RREP).
// The widths are not given (fidelity "ch. 4: header field widths", unknown);
// this layout is a repo choice under org rule 3:
//
//   byte  0      wire version (kPacketWireVersion)
//   byte  1      type (1 data, 2 hello, 3 seed, 4 RREQ, 5 RREP)
//   bytes 2-5    source       i32 LE  (the node that created the packet)
//   bytes 6-9    destination  i32 LE
//   bytes 10-13  previous hop i32 LE  (the node that sent this copy)
//   bytes 14-17  next hop     i32 LE  (the node this copy is addressed to)
//   bytes 18-21  message ID   u32 LE  (per source; with source, the duplicate key)
//   byte  22     TTL          u8      (hops the packet may still travel)
//
// Previous and next hop "may be taken from the MAC header" (p. 75); carrying
// them keeps the core independent of any MAC. Changing a field or its units
// bumps kPacketWireVersion.
#ifndef TERMITE_CORE_PACKET_H
#define TERMITE_CORE_PACKET_H

#include "swarm-observer.h"
#include "swarm-types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace termite::core {

using swarm::core::kInvalidAddress;
using swarm::core::NodeAddress;

/// Bumped whenever a field, its order or its units change (org rule 3).
inline constexpr std::uint8_t kPacketWireVersion = 1;
/// Encoded size of every header: the format has no variable-length part.
inline constexpr std::size_t kPacketWireSize = 23;

/// The five packet types of [R05] §4.2.6. This slice routes Data; the control
/// types are decoded and deposit pheromone (eq. 4.1 applies to every packet).
enum class PacketType : std::uint8_t {
    Data = 1,
    Hello = 2,
    Seed = 3,
    RouteRequest = 4,
    RouteReply = 5,
};

/// The names of the type bytes, for swarm-core's observer: readers print
/// "RREQ", not "4" (swarm::core::findKindName). Data is named too, but a data
/// packet's header is data overhead, never reported as control (docs/observer.md).
inline constexpr std::array<swarm::core::KindName, 5> kPacketKindNames{{
    {static_cast<std::uint8_t>(PacketType::Data), "DATA"},
    {static_cast<std::uint8_t>(PacketType::Hello), "HELLO"},
    {static_cast<std::uint8_t>(PacketType::Seed), "SEED"},
    {static_cast<std::uint8_t>(PacketType::RouteRequest), "RREQ"},
    {static_cast<std::uint8_t>(PacketType::RouteReply), "RREP"},
}};

struct PacketHeader {
    PacketType type = PacketType::Data;
    NodeAddress source = kInvalidAddress;
    NodeAddress destination = kInvalidAddress;
    NodeAddress previousHop = kInvalidAddress;
    NodeAddress nextHop = kInvalidAddress;
    std::uint32_t messageId = 0;
    std::uint8_t ttl = 0;

    bool operator==(const PacketHeader&) const = default;
};

/// Appends the header's kPacketWireSize bytes to `out`.
void encodePacket(const PacketHeader& header, std::vector<std::uint8_t>& out);

/// Decodes exactly one header from untrusted bytes: nullopt on a wrong
/// version, an unknown type, a short buffer or trailing bytes.
std::optional<PacketHeader> decodePacket(std::span<const std::uint8_t> bytes);

}  // namespace termite::core

#endif  // TERMITE_CORE_PACKET_H
