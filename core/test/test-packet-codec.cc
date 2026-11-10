// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// The header wire format (org rule 3): a versioned, fixed-size, little-endian
// POD; deterministic roundtrip and seeded fuzz.
#include "termite-core-tests.h"

#include <cstdint>
#include <vector>

namespace termite::core::test {

void TestPacketRoundtrip(Checker& c) {
    const PacketHeader data{PacketType::Data, 0x01020304, -2, 7, 8, 0xA1B2C3D4u, 32};
    std::vector<std::uint8_t> bytes;
    encodePacket(data, bytes);
    c.check(bytes.size() == kPacketWireSize, "fixed encoded size");
    const std::vector<std::uint8_t> expected{kPacketWireVersion,
                                             1,
                                             0x04,
                                             0x03,
                                             0x02,
                                             0x01,
                                             0xFE,
                                             0xFF,
                                             0xFF,
                                             0xFF,
                                             7,
                                             0,
                                             0,
                                             0,
                                             8,
                                             0,
                                             0,
                                             0,
                                             0xD4,
                                             0xC3,
                                             0xB2,
                                             0xA1,
                                             32};
    c.check(bytes == expected, "layout: version, type, then little-endian fields");
    const auto back = decodePacket(bytes);
    c.check(back && *back == data, "data header round-trips");

    for (PacketType t :
         {PacketType::Hello, PacketType::Seed, PacketType::RouteRequest, PacketType::RouteReply}) {
        const PacketHeader h{t, 9, 1, -1, 0x7FFFFFFF, 0, 255};
        bytes.clear();
        encodePacket(h, bytes);
        const auto again = decodePacket(bytes);
        c.check(again && *again == h, "every control type round-trips, extremes included");
    }
}

void TestPacketDecoderFailsClosed(Checker& c) {
    std::vector<std::uint8_t> good;
    encodePacket(PacketHeader{PacketType::Data, 1, 2, 3, 4, 5, 6}, good);

    auto bad = good;
    bad[0] = kPacketWireVersion + 1;
    c.check(!decodePacket(bad), "another wire version is rejected");
    for (std::uint8_t type : {0, 6, 255}) {
        bad = good;
        bad[1] = type;
        c.check(!decodePacket(bad), "an unknown packet type is rejected");
    }
    for (std::size_t n = 0; n < good.size(); ++n) {
        const std::vector<std::uint8_t> shortBytes(good.begin(),
                                                   good.begin() + static_cast<long>(n));
        c.check(!decodePacket(shortBytes), "a truncated header is rejected");
    }
    bad = good;
    bad.push_back(0);
    c.check(!decodePacket(bad), "trailing bytes are rejected");
}

void TestPacketFuzz(Checker& c) {
    SeededRng rng(20050);
    int accepted = 0;
    for (int i = 0; i < 20000; ++i) {
        std::vector<std::uint8_t> bytes;
        if (i % 2 == 0) {
            // Mutations of a valid header: flip one to three bytes.
            encodePacket(
                PacketHeader{PacketType::Data, 11, 22, 33, 44, static_cast<std::uint32_t>(i), 30},
                bytes);
            const int flips = 1 + rng.uniformInt(3);
            for (int f = 0; f < flips; ++f) {
                bytes[static_cast<std::size_t>(rng.uniformInt(static_cast<int>(bytes.size())))] =
                    static_cast<std::uint8_t>(rng.uniformInt(256));
            }
        } else {
            // Random buffers of 0..31 bytes, the version byte usually right.
            const int n = rng.uniformInt(32);
            for (int b = 0; b < n; ++b)
                bytes.push_back(static_cast<std::uint8_t>(rng.uniformInt(256)));
            if (!bytes.empty() && rng.uniformInt(4) != 0) bytes[0] = kPacketWireVersion;
        }
        const auto header = decodePacket(bytes);
        if (!header) continue;
        ++accepted;
        std::vector<std::uint8_t> again;
        encodePacket(*header, again);
        if (again != bytes) {
            c.check(false, "an accepted buffer re-encodes to itself");
            return;
        }
    }
    c.check(accepted > 1000, "the fuzz reaches the accepting path");
}

}  // namespace termite::core::test
