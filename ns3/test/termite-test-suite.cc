// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
//
// `test.py -s termite`. Needs ns-3.42+ (scoped TestCase::Duration).
//
// - core/'s tests (core/test/), one TestCase each, so the debug and sanitized
//   legs run them inside ns-3 as core.yml runs them alone;
// - the ns-3 header codec: roundtrip through a Packet, and a seeded fuzz of
//   the reader a received data packet goes through (org rule 3);
// - the adapter on static 802.11b topologies. The core's first slice has no
//   hello, seed or route request (#6), so a fresh network knows no
//   destination; each test first lays a trail as a packet of the destination
//   would, with raw frames sent through packet sockets (real frames, through
//   the receivers' RouteInput and their neighbours' promiscuous reception).
//   Then: data delivered over the trail with the header on the air and off
//   the delivered payload, the reply over the trail the data laid, a packet
//   to an unknown destination dropped rather than held, an overheard frame
//   depositing once, multipath forwarding (eq. 4.3);
// - determinism: the same seed gives the same decision trace, another seed
//   another (org rule 5).
// Through the include path (core/, resolved past the contrib link), not
// "../../core/...": gcov would collapse that beside the link, not its target.
#include "test/termite-core-tests.h"
#include "../model/termite-header.h"

#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/mobility-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/swarm-core-module.h"
#include "ns3/termite-helper.h"
#include "ns3/termite-routing-protocol.h"
#include "ns3/test.h"
#include "ns3/udp-header.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-helper.h"
#include "ns3/yans-wifi-helper.h"

#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;
using swarm::core::RouteAction;
using ::termite::core::PacketHeader;
using ::termite::core::PacketType;

namespace {

/// One core/ test function, its failures reported through ns-3.
class TermiteCoreTestCase : public TestCase {
public:
    explicit TermiteCoreTestCase(const ::termite::core::test::NamedTest& t)
        : TestCase(std::string("core: ") + t.name), m_test(t) {}

private:
    void DoRun() override {
        ::termite::core::test::Checker c;
        m_test.run(c);
        // Each failure prints as "got <file:line: check>, expected ''".
        for (const auto& f : c.failures()) {
            NS_TEST_EXPECT_MSG_EQ(f, std::string(), "core check failed");
        }
    }

    ::termite::core::test::NamedTest m_test;
};

std::vector<uint8_t> Bytes(Ptr<const Packet> p) {
    std::vector<uint8_t> b(p->GetSize());
    p->CopyData(b.data(), static_cast<uint32_t>(b.size()));
    return b;
}

Ptr<Packet> FromBytes(const std::vector<uint8_t>& b) {
    if (b.empty()) return Create<Packet>();  // data() may be null
    return Create<Packet>(b.data(), static_cast<uint32_t>(b.size()));
}

class HeaderRoundtripTestCase : public TestCase {
public:
    HeaderRoundtripTestCase() : TestCase("header: TermiteHeader roundtrip through a Packet") {}

private:
    void DoRun() override {
        const std::vector<PacketHeader> headers{
            {PacketType::Data, 0x0A010001, 0x0A010004, 0x0A010002, 0x0A010003, 1, 32},
            {PacketType::RouteReply, -2, 0x7FFFFFFF, 1, 2, 0xFFFFFFFFu, 0},
        };
        const Ptr<Packet> payload = Create<Packet>(64);
        for (const PacketHeader& h : headers) {
            Ptr<Packet> p = ns3::termite::AddPacketHeader(payload, h);
            NS_TEST_ASSERT_MSG_EQ(p->GetSize(), ::termite::core::kPacketWireSize + 64,
                                  "the header in front of the payload");
            NS_TEST_EXPECT_MSG_EQ(Bytes(p)[0], ::termite::core::kPacketWireVersion,
                                  "leading version byte");
            const auto read = ns3::termite::ReadPacketHeader(p);
            NS_TEST_ASSERT_MSG_EQ(read.has_value(), true, "reads");
            NS_TEST_EXPECT_MSG_EQ((*read == h), true, "same header back");
            NS_TEST_EXPECT_MSG_EQ(ns3::termite::StripPacketHeader(p)->GetSize(), 64u,
                                  "stripped back to the payload");
            ns3::termite::TermiteHeader th;
            NS_TEST_EXPECT_MSG_EQ(p->Copy()->RemoveHeader(th), ::termite::core::kPacketWireSize,
                                  "RemoveHeader consumes the header");
            NS_TEST_EXPECT_MSG_EQ(th.IsValid(), true, "header valid");
            NS_TEST_EXPECT_MSG_EQ((th.Get() == h), true, "same header through TermiteHeader");
        }
        // The core's byte layout, little endian (core/termite-packet.h).
        const auto b = Bytes(ns3::termite::AddPacketHeader(Create<Packet>(), headers[0]));
        const std::vector<uint8_t> want{1,    1,    0x01, 0x00, 0x01, 0x0A, 0x04, 0x00,
                                        0x01, 0x0A, 0x02, 0x00, 0x01, 0x0A, 0x03, 0x00,
                                        0x01, 0x0A, 1,    0,    0,    0,    32};
        NS_TEST_EXPECT_MSG_EQ((b == want), true, "wire layout");
        // A wrong version: refused, and invalid through the header.
        std::vector<uint8_t> bad = want;
        bad[0] = ::termite::core::kPacketWireVersion + 1;
        NS_TEST_EXPECT_MSG_EQ(ns3::termite::ReadPacketHeader(FromBytes(bad)).has_value(), false,
                              "a wrong version is refused");
        ns3::termite::TermiteHeader th;
        FromBytes(bad)->RemoveHeader(th);
        NS_TEST_EXPECT_MSG_EQ(th.IsValid(), false, "a wrong version is invalid");
    }
};

/// SplitMix64, as core/'s tests: the same bytes on every platform.
uint64_t Mix(uint64_t& s) {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

class HeaderFuzzTestCase : public TestCase {
public:
    HeaderFuzzTestCase() : TestCase("header: seeded fuzz of the data-packet header reader") {}

private:
    void DoRun() override {
        uint64_t s = 0x7E57;
        std::vector<uint8_t> valid;
        ::termite::core::encodePacket({PacketType::Data, 7, 9, 3, 4, 42, 30}, valid);
        uint32_t accepted = 0;
        for (int i = 0; i < 4000; ++i) {
            std::vector<uint8_t> b;
            if (i % 2 == 0) {
                // Random bytes, 0 to 40 long.
                b.resize(Mix(s) % 41);
                for (auto& x : b)
                    x = static_cast<uint8_t>(Mix(s));
                if (!b.empty() && i % 4 == 0) b[0] = ::termite::core::kPacketWireVersion;
            } else {
                // A valid header with one byte changed, cut short or followed by a payload.
                b = valid;
                b[Mix(s) % b.size()] = static_cast<uint8_t>(Mix(s));
                const uint64_t shape = Mix(s) % 3;
                if (shape == 1) b.resize(Mix(s) % b.size());
                if (shape == 2) b.resize(b.size() + Mix(s) % 16, static_cast<uint8_t>(Mix(s)));
            }
            const auto h = ns3::termite::ReadPacketHeader(FromBytes(b));
            if (!h) continue;
            ++accepted;
            // Whatever is accepted is one canonical header followed by the rest.
            NS_TEST_ASSERT_MSG_GT_OR_EQ(b.size(), ::termite::core::kPacketWireSize, "size");
            std::vector<uint8_t> again;
            ::termite::core::encodePacket(*h, again);
            NS_TEST_ASSERT_MSG_EQ(
                (std::vector<uint8_t>(b.begin(), b.begin() + again.size()) == again), true,
                "accepted bytes re-encode identically");
        }
        NS_TEST_EXPECT_MSG_GT(accepted, 0u, "the fuzz reaches the accepting path");
        NS_TEST_EXPECT_MSG_EQ(ns3::termite::ReadPacketHeader(Create<Packet>()).has_value(), false,
                              "empty payload");
        std::vector<uint8_t> type = valid;
        type[1] = 6;
        NS_TEST_EXPECT_MSG_EQ(ns3::termite::ReadPacketHeader(FromBytes(type)).has_value(), false,
                              "unknown type");
        std::vector<uint8_t> shortened(valid.begin(), valid.end() - 1);
        NS_TEST_EXPECT_MSG_EQ(ns3::termite::ReadPacketHeader(FromBytes(shortened)).has_value(),
                              false, "one byte short");
    }
};

// --- the adapter on static topologies ---------------------------------------------

constexpr uint16_t kDataPort = 9;
constexpr uint16_t kTrailPort = 7;  ///< where the trail-laying frames end; a discarding sink
constexpr uint32_t kPayload = 64;
constexpr uint16_t kIpv4EtherType = 0x0800;

/// A static 802.11b network with Termite on every node, packet sockets to lay
/// trails, a data sink on every node, and a trace of every data decision.
class Net {
public:
    Net(const std::vector<Vector>& positions, uint32_t seed) {
        SwarmCoreSeed(seed);
        m_nodes.Create(positions.size());
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
        for (const Vector& v : positions)
            pos->Add(v);
        mobility.SetPositionAllocator(pos);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(m_nodes);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                                     StringValue("DsssRate2Mbps"), "ControlMode",
                                     StringValue("DsssRate1Mbps"));
        YansWifiChannelHelper channel;
        channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange",
                                   DoubleValue(250.0));
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());
        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        m_devs = wifi.Install(phy, mac, m_nodes);

        TermiteHelper termite;
        termite.Install(m_nodes);  // the IPv4 stack brings the packet-socket factory
        Ipv4AddressHelper a;
        a.SetBase("10.1.1.0", "255.255.255.0");
        m_ifs = a.Assign(m_devs);
        // Every stream pinned, so a second run in this process draws as the first.
        int64_t stream = termite.AssignStreams(m_nodes, 0);
        stream += WifiHelper::AssignStreams(m_devs, stream);
        InternetStackHelper().AssignStreams(m_nodes, stream);

        received.assign(positions.size(), 0);
        for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
            Proto(i)->TraceConnect("Data", std::to_string(i), MakeCallback(&Net::Data, this));
            for (uint16_t port : {kDataPort, kTrailPort}) {
                Ptr<Socket> s = Socket::CreateSocket(m_nodes.Get(i), UdpSocketFactory::GetTypeId());
                s->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
                s->SetIpRecvTtl(true);
                if (port == kDataPort) {
                    s->SetRecvCallback(MakeBoundCallback(&Net::Receive, this, i));
                } else {
                    s->SetRecvCallback(MakeCallback(&Net::Discard));
                }
                m_sockets.push_back(s);
            }
        }
    }

    ~Net() {
        for (auto& s : m_sockets)
            s->Close();
        m_sockets.clear();
        Simulator::Destroy();
    }

    Ptr<ns3::termite::RoutingProtocol> Proto(uint32_t i) const {
        return m_nodes.Get(i)->GetObject<ns3::termite::RoutingProtocol>();
    }
    Ipv4Address Addr(uint32_t i) const { return m_ifs.GetAddress(i); }
    Ptr<Ipv4L3Protocol> L3(uint32_t i) const { return m_nodes.Get(i)->GetObject<Ipv4L3Protocol>(); }

    /// At `at`, node `from` sends the frame a Termite node would: `h` in front
    /// of a UDP datagram to kTrailPort, in an IPv4 packet from h.source to
    /// h.destination, to node `to`'s MAC address. No routing at `from`.
    void Lay(double at, uint32_t from, uint32_t to, const PacketHeader& h) {
        Simulator::Schedule(Seconds(at), &Net::SendRaw, this, from, to, h);
    }

    /// Node `from` sends `count` data packets of kPayload bytes to `to`.
    void Send(double at, double interval, uint32_t count, uint32_t from, Ipv4Address to) {
        Ptr<Socket> s = Socket::CreateSocket(m_nodes.Get(from), UdpSocketFactory::GetTypeId());
        m_sockets.push_back(s);
        for (uint32_t k = 0; k < count; ++k) {
            Simulator::Schedule(Seconds(at + interval * k), &Net::SendData, this, s, to);
        }
    }

    void Run(double until) {
        Simulator::Stop(Seconds(until));
        Simulator::Run();
    }

    ::termite::core::NodeAddress Core(uint32_t i) const { return ToNodeAddress(Addr(i)); }

    std::vector<std::string> trace;     ///< every data decision, in order
    std::vector<uint32_t> received;     ///< data packets received per node
    std::set<uint32_t> ttls;            ///< IPv4 TTLs data packets arrived with
    std::set<uint32_t> sizes;           ///< sizes of the received data payloads
    std::vector<uint8_t> actions;       ///< RouteAction of each decision, in order
    std::vector<uint32_t> actionNodes;  ///< the node of each decision

private:
    void SendRaw(uint32_t from, uint32_t to, PacketHeader h) {
        Ptr<Packet> p = Create<Packet>(8);
        UdpHeader udp;
        udp.SetSourcePort(kTrailPort);
        udp.SetDestinationPort(kTrailPort);
        p->AddHeader(udp);
        p->AddHeader(ns3::termite::TermiteHeader(h));
        Ipv4Header ip;
        ip.SetSource(ToIpv4Address(h.source));
        ip.SetDestination(ToIpv4Address(h.destination));
        ip.SetProtocol(17);
        ip.SetTtl(64);
        ip.SetPayloadSize(p->GetSize());
        p->AddHeader(ip);
        Ptr<NetDevice> dev = m_devs.Get(from);
        Ptr<Socket> s = Socket::CreateSocket(m_nodes.Get(from),
                                             TypeId::LookupByName("ns3::PacketSocketFactory"));
        PacketSocketAddress addr;
        addr.SetSingleDevice(dev->GetIfIndex());
        addr.SetPhysicalAddress(m_devs.Get(to)->GetAddress());
        addr.SetProtocol(kIpv4EtherType);
        s->Bind(addr);
        s->SendTo(p, 0, addr);
        m_sockets.push_back(s);
    }

    void SendData(Ptr<Socket> s, Ipv4Address to) {
        s->SendTo(Create<Packet>(kPayload), 0, InetSocketAddress(to, kDataPort));
    }

    static void Receive(Net* net, uint32_t node, Ptr<Socket> s) {
        while (Ptr<Packet> p = s->Recv()) {
            ++net->received[node];
            net->sizes.insert(p->GetSize());
            SocketIpTtlTag ttl;
            if (p->PeekPacketTag(ttl)) net->ttls.insert(ttl.GetTtl());
        }
    }

    static void Discard(Ptr<Socket> s) {
        while (s->Recv()) {
        }
    }

    void Data(std::string node, Ipv4Address source, uint32_t id, uint8_t action, Ipv4Address next) {
        std::ostringstream os;
        os << Simulator::Now().GetNanoSeconds() << " n" << node << " src=" << source << " id=" << id
           << " action=" << int{action} << " next=" << next;
        trace.push_back(os.str());
        actions.push_back(action);
        actionNodes.push_back(static_cast<uint32_t>(std::stoul(node)));
    }

    NodeContainer m_nodes;
    NetDeviceContainer m_devs;
    Ipv4InterfaceContainer m_ifs;
    std::vector<Ptr<Socket>> m_sockets;
};

/// A trail-laying header: the frame `prev` sends `next`, of a packet `source`
/// sent to `next` (the frame ends there).
PacketHeader Trail(const Net& n, uint32_t source, uint32_t prev, uint32_t next, uint32_t id,
                   uint8_t ttl) {
    return {PacketType::Data, n.Core(source), n.Core(next), n.Core(prev), n.Core(next), id, ttl};
}

constexpr uint32_t kTrailId = 0xF0000000u;  ///< above any ID a node's own packets reach here

/// Ipv4L3Protocol "Tx": keeps the first UDP packet sent on a radio interface
/// (interface 0 is the loopback), IPv4 header included.
void FirstDataTx(std::vector<uint8_t>* out, Ptr<const Packet> p, Ptr<Ipv4>, uint32_t iface) {
    if (iface == 0 || !out->empty()) return;
    Ipv4Header ip;
    p->PeekHeader(ip);
    if (ip.GetProtocol() == 17) *out = Bytes(p);
}

/// Ipv4L3Protocol "Drop": counts the drops a routing protocol reported.
void RouteDrop(uint32_t* drops, const Ipv4Header&, Ptr<const Packet>, Ipv4L3Protocol::DropReason r,
               Ptr<Ipv4>, uint32_t) {
    if (r == Ipv4L3Protocol::DROP_ROUTE_ERROR) ++*drops;
}

class ChainTestCase : public TestCase {
public:
    ChainTestCase()
        : TestCase("adapter: data and reply over the trail on a 4-node chain, header on the air") {}

private:
    void DoRun() override {
        Net n({{0, 0, 0}, {200, 0, 0}, {400, 0, 0}, {600, 0, 0}}, 1);
        // A trail towards node 3, as one of its packets would lay it: 3 -> 2 -> 1 -> 0.
        n.Lay(0.1, 3, 2, Trail(n, 3, 3, 2, kTrailId, 32));
        n.Lay(0.2, 2, 1, Trail(n, 3, 2, 1, kTrailId, 31));
        n.Lay(0.3, 1, 0, Trail(n, 3, 1, 0, kTrailId, 30));
        // The first data frame node 0 puts on the air (interface 1; 0 is the loopback).
        std::vector<uint8_t> firstTx;
        n.L3(0)->TraceConnectWithoutContext("Tx", MakeBoundCallback(&FirstDataTx, &firstTx));
        uint32_t routeDrops = 0;
        n.L3(0)->TraceConnectWithoutContext("Drop", MakeBoundCallback(&RouteDrop, &routeDrops));
        n.Send(1.0, 0.25, 10, 0, n.Addr(3));
        n.Send(4.0, 0.25, 5, 3, n.Addr(0));
        n.Send(6.0, 0.25, 1, 0, Ipv4Address("10.1.1.250"));  // nobody's address
        n.Run(7.0);

        NS_TEST_EXPECT_MSG_EQ(n.received[3], 10u, "every packet 0 -> 3 delivered over the trail");
        NS_TEST_EXPECT_MSG_EQ(n.received[0], 5u, "the reply 3 -> 0 over the trail data laid");
        NS_TEST_EXPECT_MSG_EQ((n.sizes == std::set<uint32_t>{kPayload}), true,
                              "the header is off every delivered payload");
        // Three radio hops, and none counted for the source's loopback.
        NS_TEST_EXPECT_MSG_EQ((n.ttls == std::set<uint32_t>{62}), true, "IPv4 TTL 64 - 2 relays");

        // On the air: IPv4, then the core's header, then UDP and the data.
        NS_TEST_ASSERT_MSG_EQ(firstTx.size(), 20 + ::termite::core::kPacketWireSize + 8 + kPayload,
                              "IPv4 + Termite + UDP + data");
        const auto h = ns3::termite::ReadPacketHeader(
            FromBytes(std::vector<uint8_t>(firstTx.begin() + 20, firstTx.end())));
        NS_TEST_ASSERT_MSG_EQ(h.has_value(), true, "the header follows the IPv4 header");
        const PacketHeader want{PacketType::Data, n.Core(0), n.Core(3), n.Core(0),
                                n.Core(1),        0,         31};
        NS_TEST_EXPECT_MSG_EQ((*h == want), true, "source 0, to 3, next hop 1, ID 0, TTL 32 - 1");

        // The packet to an unknown destination: dropped at once, never held.
        NS_TEST_EXPECT_MSG_EQ(routeDrops, 1u, "one route drop at node 0");
        NS_TEST_EXPECT_MSG_EQ(n.Proto(0)->GetDeferredQueue()->Stats().enqueued, 0u,
                              "nothing held in the deferred queue");
        uint32_t queued = 0;
        for (std::size_t i = 0; i < n.actions.size(); ++i) {
            if (n.actions[i] == static_cast<uint8_t>(RouteAction::Queue) && n.actionNodes[i] == 0)
                ++queued;
        }
        NS_TEST_EXPECT_MSG_EQ(queued, 1u, "the core asked to queue it");
        // The data laid the reverse trail: node 2 reaches 0 through 1.
        NS_TEST_EXPECT_MSG_GT(n.Proto(2)->Pheromone(n.Addr(0), n.Addr(1)), 0.0, "P(0 via 1) at 2");
    }
};

class OverhearTestCase : public TestCase {
public:
    OverhearTestCase() : TestCase("adapter: an overheard data frame deposits once (§4.2.2)") {}

private:
    void DoRun() override {
        Net n({{0, 0, 0}, {100, 0, 0}, {50, 80, 0}}, 1);
        // Node 0's packet to node 1, sent twice (a duplicate copy); node 2 overhears both.
        n.Lay(0.1, 0, 1, Trail(n, 0, 0, 1, 7, 32));
        n.Lay(0.2, 0, 1, Trail(n, 0, 0, 1, 7, 32));
        double heard = -1;
        double addressed = -1;
        Simulator::Schedule(Seconds(0.5), [&] {
            heard = n.Proto(2)->Pheromone(n.Addr(0), n.Addr(0));
            addressed = n.Proto(1)->Pheromone(n.Addr(0), n.Addr(0));
        });
        n.Run(0.6);
        // Before the first decay tick (1 s): the initial 1 plus one deposit of 1.
        NS_TEST_EXPECT_MSG_EQ_TOL(addressed, 2.0, 1e-12, "the addressee deposits once");
        NS_TEST_EXPECT_MSG_EQ_TOL(heard, 2.0, 1e-12, "the overhearer deposits once");
        for (uint32_t node : n.actionNodes) {
            NS_TEST_EXPECT_MSG_NE(node, 2u, "an overheard frame is never acted on");
        }
    }
};

/// Node 0 sends to node 3 across a diamond whose two middle nodes both carry
/// a trail of 3's, with equal pheromone: eq. 4.3 draws between them.
std::unique_ptr<Net> Diamond(uint32_t seed) {
    auto n = std::make_unique<Net>(
        std::vector<Vector>{{0, 0, 0}, {150, 100, 0}, {150, -100, 0}, {300, 0, 0}}, seed);
    n->Lay(0.1, 3, 1, Trail(*n, 3, 3, 1, kTrailId, 32));
    n->Lay(0.2, 3, 2, Trail(*n, 3, 3, 2, kTrailId + 1, 32));
    n->Lay(0.3, 1, 0, Trail(*n, 3, 1, 0, kTrailId, 31));
    n->Lay(0.4, 2, 0, Trail(*n, 3, 2, 0, kTrailId + 1, 31));
    n->Send(1.0, 0.25, 12, 0, n->Addr(3));
    n->Run(5.0);
    return n;
}

class DeterminismTestCase : public TestCase {
public:
    DeterminismTestCase()
        : TestCase("determinism: multipath on a diamond, same seed same trace, another seed "
                   "another") {}

private:
    void DoRun() override {
        std::vector<std::string> a;
        std::vector<std::string> b;
        std::vector<std::string> c;
        std::set<std::string> firstHops;
        uint32_t delivered = 0;
        {
            const auto n = Diamond(1);
            a = n->trace;
            delivered = n->received[3];
            for (const auto& line : n->trace) {
                // Node 0's own packets: the next hop eq. 4.3 drew.
                if (line.find(" n0 src=10.1.1.1 ") != std::string::npos)
                    firstHops.insert(line.substr(line.find("next=")));
            }
        }
        {
            const auto n = Diamond(1);
            b = n->trace;
        }
        {
            const auto n = Diamond(2);
            c = n->trace;
        }
        NS_TEST_ASSERT_MSG_GT(a.size(), 0u, "something happened");
        NS_TEST_EXPECT_MSG_EQ(delivered, 12u, "every packet delivered");
        NS_TEST_EXPECT_MSG_EQ(firstHops.size(), 2u, "node 0 used both middle nodes");
        NS_TEST_EXPECT_MSG_EQ((a == b), true, "seed 1 twice: identical");
        NS_TEST_EXPECT_MSG_EQ((a != c), true, "seed 2: a different trace");
    }
};

class TermiteTestSuite : public TestSuite {
public:
    TermiteTestSuite() : TestSuite("termite", Type::UNIT) {
        for (const auto& t : ::termite::core::test::AllTests()) {
            AddTestCase(new TermiteCoreTestCase(t), TestCase::Duration::QUICK);
        }
        AddTestCase(new HeaderRoundtripTestCase, TestCase::Duration::QUICK);
        AddTestCase(new HeaderFuzzTestCase, TestCase::Duration::QUICK);
        AddTestCase(new ChainTestCase, TestCase::Duration::QUICK);
        AddTestCase(new OverhearTestCase, TestCase::Duration::QUICK);
        AddTestCase(new DeterminismTestCase, TestCase::Duration::QUICK);
    }
};

TermiteTestSuite g_termiteTestSuite;

}  // namespace
