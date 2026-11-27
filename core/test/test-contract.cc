// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// swarm-core's protocol contract suite (testbench/swarm-contract-tests.h, its
// README "Protocol contract suite: how a protocol opts in"; #185): Termite's
// router on swarm-core's testbench, a plain ctest executable.
//
// The adapter puts every frame on the air as its wire bytes (the codec on
// every hop) and as a one-hop broadcast: every neighbour of the sender hears
// it, the addressed one (header.nextHop) acts on it and the others overhear
// it (promiscuous reception, [R05] §4.2.2), as the router expects. The
// router's route-discovery sends (IRouterOutput, #200) go on the air the same
// way. The core takes no link-state, link-quality or energy port; of
// ContractProviders it takes only the observer.
//
// A fresh network puts nothing on the air yet: a router learns a neighbour
// only from a frame it hears, and hello, which would introduce them, is the
// next slice (#201; AGENTS.md status). Until then the scene stands in for it:
// at time 0 every node is handed one Hello frame from each neighbour (source
// the neighbour, an ID no node originates), as a hello beacon would be heard.
// The router deposits for it like any frame ([R05] §4.2.2) and so learns the
// neighbour; nothing else is seeded: every route is still found by a route
// request. The checks are swarm-core's, unchanged.
//
// The scene: a 3 x 3 grid (nodes 0..8 row by row; the suite's probe needs
// the link 0 - 1 and sends toward node 8), where node 0 and node 1 send data
// to node 8 and node 8 to node 1, a packet every 0.5 s each. Every
// destination is unknown at first, so every flow starts with a route request
// (the RREQ walks draw from the node's IRng, as does eq. 4.3 where a node has
// more than one candidate): the seed reaches the air.
//
// The hop-limit check hands node 1 three Data messages from node 0 at 5 s:
// a live one (control case), one with TTL 0, and one whose source is node 1
// itself. Termite has no "source is this node" rule: a message back at its
// origin is dropped because its ID is in the origin's duplicate list ([R05]
// §4.3.2; the source records its own packet as forwarded, fidelity "ch. 4:
// no next hop although the destination is known"). So the message hopLimited
// builds for origin 1 is node 1's own first one (ID 0 toward node 8, which
// the scene sends at 0.5 s), as the check describes it: "its own message come
// back". For any other origin it carries an ID no node originates
// (kProbeId), so the live message is new to node 1 and is forwarded.
#include "../termite-packet.h"
#include "../termite-router.h"

#include "testbench/swarm-contract-tests.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace tb = swarm::core::testbench;
using swarm::core::NodeAddress;
using swarm::core::RouteAction;
using termite::core::Config;
using termite::core::Decision;
using termite::core::PacketHeader;
using termite::core::PacketType;
using termite::core::Router;

namespace {

using Frame = std::vector<std::uint8_t>;

/// A message ID no node of the scene originates (each counts from 0).
constexpr std::uint32_t kProbeId = 0x40000000U;
/// The ID of the stand-in hello frames (another one no node originates).
constexpr std::uint32_t kHelloId = 0x20000000U;
constexpr int kSide = 3;
constexpr swarm::core::Time kFirstSend = 0.5;
constexpr swarm::core::Time kInterval = 0.5;

Frame encode(const PacketHeader& h) {
    Frame bytes;
    termite::core::encodePacket(h, bytes);
    return bytes;
}

std::uint64_t fnv1a(const Frame& bytes) {
    std::uint64_t h = 0xCBF29CE484222325ULL;
    for (std::uint8_t b : bytes) {
        h ^= b;
        h *= 0x100000001B3ULL;
    }
    return h;
}

/// One Termite node on the testbench: the router, its output port, and the
/// scene's traffic from this node.
class TermiteNode : public tb::INodeAdapter<Frame>, public termite::core::IRouterOutput {
public:
    TermiteNode(tb::NodePorts<Frame>& ports, const tb::ContractProviders& providers,
                std::vector<NodeAddress> sendsTo, swarm::core::Time until)
        : ports_(ports), router_(ports.address(), Config{}, ports.rng(), ports.timers()),
          sendsTo_(std::move(sendsTo)), until_(until) {
        router_.setObserver(providers.observer);
        router_.setOutput(this);
    }

    void start() override {
        router_.start();
        if (!sendsTo_.empty()) ports_.schedule(kFirstSend, [this] { sendAll(); });
    }

    void receive(NodeAddress /*from*/, const Frame& msg) override {
        // The codec on every hop: a frame that does not decode is dropped.
        if (auto h = termite::core::decodePacket(msg)) carry(router_.onReceive(*h));
    }

    void sendControl(const Decision& d) override { carry(d); }
    void release(NodeAddress /*destination*/, const std::vector<Decision>& ds) override {
        for (const Decision& d : ds)
            carry(d);
    }
    void discard(NodeAddress /*destination*/, std::size_t /*count*/) override {}

private:
    void sendAll() {
        for (NodeAddress to : sendsTo_)
            carry(router_.originate(to));
        if (ports_.clock().now() + kInterval < until_)
            ports_.schedule(kInterval, [this] { sendAll(); });
    }

    /// A Unicast goes on the air; every other action stays at the node.
    void carry(const Decision& d) {
        if (d.action == RouteAction::Unicast && d.message) ports_.broadcast(encode(*d.message));
    }

    tb::NodePorts<Frame>& ports_;
    Router router_;
    std::vector<NodeAddress> sendsTo_;
    swarm::core::Time until_;
};

tb::Topology grid() {
    tb::Topology t(kSide * kSide);
    for (int r = 0; r < kSide; ++r) {
        for (int c = 0; c < kSide; ++c) {
            const int n = kSide * r + c;
            if (c + 1 < kSide) t.connect(n, n + 1);
            if (r + 1 < kSide) t.connect(n, n + kSide);
        }
    }
    return t;
}

}  // namespace

int main() {
    tb::ContractSpec<Frame> spec;
    spec.topology = grid();
    const NodeAddress last = kSide * kSide - 1;
    const swarm::core::Time until = spec.duration;
    spec.factory = [last, until](tb::NodePorts<Frame>& p, const tb::ContractProviders& prov)
        -> std::unique_ptr<tb::INodeAdapter<Frame>> {
        std::vector<NodeAddress> sendsTo;
        if (p.address() == 0 || p.address() == 1) sendsTo.push_back(last);
        if (p.address() == last) sendsTo.push_back(1);
        return std::make_unique<TermiteNode>(p, prov, std::move(sendsTo), until);
    };
    spec.script = [](tb::Network<Frame>& net) {
        for (NodeAddress a = 0; a < static_cast<NodeAddress>(net.size()); ++a) {
            for (NodeAddress n : net.topology().neighbors(a)) {
                PacketHeader hello;
                hello.type = PacketType::Hello;
                hello.source = n;
                hello.destination = a;
                hello.previousHop = n;
                hello.nextHop = a;
                hello.messageId = kHelloId;
                hello.ttl = 1;
                net.at(0.0, [&net, a, n, frame = encode(hello)] { net.node(a).receive(n, frame); });
            }
        }
    };
    spec.kindOf = [](const Frame& f) -> std::uint8_t {
        return f.size() > 1 ? f[1] : 0;  // the type byte, after the version byte
    };
    spec.payloadHash = fnv1a;
    spec.hopLimited = [](NodeAddress origin, NodeAddress dest, int hopLimit) {
        PacketHeader h;
        h.type = PacketType::Data;
        h.source = origin;
        h.destination = dest;
        h.previousHop = 0;  // the suite hands every probe to node 1 as from node 0
        h.nextHop = 1;
        h.messageId = origin == 1 ? 0U : kProbeId;  // node 1's own first message (see above)
        h.ttl = static_cast<std::uint8_t>(hopLimit);
        return encode(h);
    };
    return tb::runProtocolContract(spec) == 0 ? 0 : 1;
}
