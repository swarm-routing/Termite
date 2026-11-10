// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// Dependency-free test support for Termite's core tests: a failure collector,
// a seeded RNG, a discrete-event scheduler behind swarm-core's ports, and a
// static radio network where every frame crosses the codec and is heard by
// every neighbour of its sender (promiscuous reception). The runners
// (run-core-tests.cc here; the ns-3 suite later) report what the Checker
// collects.
#ifndef TERMITE_CORE_TEST_SUPPORT_H
#define TERMITE_CORE_TEST_SUPPORT_H

#include "../termite-packet.h"
#include "../termite-router.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace termite::core::test {

using swarm::core::RouteAction;

/// Collects failed checks, each with its file and line.
class Checker {
public:
    void check(bool ok, std::string_view what,
               std::source_location where = std::source_location::current()) {
        if (ok) return;
        failures_.push_back(std::string(where.file_name()) + ":" + std::to_string(where.line()) +
                            ": " + std::string(what));
    }
    const std::vector<std::string>& failures() const { return failures_; }

private:
    std::vector<std::string> failures_;
};

/// SplitMix64: fully specified, so a seed gives the same sequence everywhere.
class SeededRng : public swarm::core::IRng {
public:
    explicit SeededRng(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double uniform() override {
        ++draws_;
        return static_cast<double>(next() >> 11) * 0x1.0p-53;
    }
    int uniformInt(int n) override {
        if (n <= 0) return 0;
        ++draws_;
        return static_cast<int>(next() % static_cast<std::uint64_t>(n));
    }
    int draws() const { return draws_; }

private:
    std::uint64_t state_;
    int draws_ = 0;
};

/// The timer port as one discrete-event queue: events run in (time,
/// scheduling order), so a run is fully deterministic.
class EventQueue : public swarm::core::ITimerScheduler {
public:
    swarm::core::Time now() const { return now_; }
    void schedule(swarm::core::Time delay, std::function<void()> callback) override {
        events_.push(Event{now_ + (delay > 0.0 ? delay : 0.0), next_++, std::move(callback)});
    }
    /// Runs every event due at or before `until`, then sets the clock to it.
    void runUntil(swarm::core::Time until) {
        while (!events_.empty() && events_.top().at <= until) {
            Event e = events_.top();
            events_.pop();
            now_ = e.at;
            e.callback();
        }
        if (until > now_) now_ = until;
    }
    std::size_t pending() const { return events_.size(); }

private:
    struct Event {
        swarm::core::Time at;
        std::uint64_t order;
        std::function<void()> callback;
        bool operator>(const Event& o) const {
            return std::tie(at, order) > std::tie(o.at, o.order);
        }
    };
    swarm::core::Time now_ = 0.0;
    std::uint64_t next_ = 0;
    std::priority_queue<Event, std::vector<Event>, std::greater<>> events_;
};

/// A single router, for unit tests.
struct Harness {
    explicit Harness(NodeAddress self, Config config = {}, std::uint64_t seed = 1)
        : rng(seed), router(self, config, rng, events) {}
    EventQueue events;
    SeededRng rng;
    Router router;
};

/// A data frame `from` sends to `to` for (source, destination, id).
inline PacketHeader dataFrame(NodeAddress source, NodeAddress destination, NodeAddress from,
                              NodeAddress to, std::uint32_t id, std::uint8_t ttl = 32) {
    return {PacketType::Data, source, destination, from, to, id, ttl};
}

/// A static radio network on bidirectional links. A unicast frame crosses
/// the codec and a link delay and reaches every neighbour of its sender: the
/// addressed one processes it, the others overhear it. The trace lines are
/// what the determinism test compares.
class Network {
public:
    explicit Network(std::uint64_t seed, Config config = {}, swarm::core::Time linkDelay = 0.001)
        : rng_(seed), config_(config), linkDelay_(linkDelay) {}

    void addLink(NodeAddress a, NodeAddress b) {
        node(a);
        node(b);
        links_[a].insert(b);
        links_[b].insert(a);
    }
    Router& node(NodeAddress a) {
        auto it = routers_.find(a);
        if (it == routers_.end())
            it = routers_.emplace(a, std::make_unique<Router>(a, config_, rng_, events_)).first;
        return *it->second;
    }
    /// Lays a trail toward path[0] along `path`, as a packet from path[0]
    /// would: each node hears a frame of path[0]'s from the node before it.
    void layTrail(const std::vector<NodeAddress>& path) {
        for (std::size_t i = 1; i < path.size(); ++i)
            node(path[i]).onReceive(
                dataFrame(path[0], path[i], path[i - 1], path[i], 1000000 + trails_++));
    }
    /// Hands `from`'s decision on `in` (the frame it received, or none for a
    /// packet it originated) to the radio.
    void carry(NodeAddress from, const Decision& d, const PacketHeader* in = nullptr) {
        if (d.action == RouteAction::None) return;  // an overheard frame
        char line[160];
        const PacketHeader* h = d.message ? &*d.message : in;
        std::snprintf(line, sizeof line, "%.6f %d %d src=%d dst=%d id=%u ttl=%u to=%d",
                      events_.now(), from, static_cast<int>(d.action), h ? h->source : -1,
                      h ? h->destination : -1, h ? h->messageId : 0u, h ? h->ttl : 0u, d.nextHop);
        trace_.emplace_back(line);
        if (d.action == RouteAction::Deliver) ++delivered_;
        if (d.action == RouteAction::Drop) ++dropped_;
        if (d.action == RouteAction::Queue) ++queued_;
        if (d.action != RouteAction::Unicast || !h) return;
        std::vector<std::uint8_t> bytes;
        encodePacket(*h, bytes);
        for (NodeAddress to : links_[from]) {
            events_.schedule(linkDelay_, [this, bytes, to] {
                if (auto frame = decodePacket(bytes))
                    carry(to, node(to).onReceive(*frame), &*frame);
            });
        }
    }
    void send(NodeAddress from, NodeAddress to) { carry(from, node(from).originate(to)); }

    EventQueue& events() { return events_; }
    const std::vector<std::string>& trace() const { return trace_; }
    int delivered() const { return delivered_; }
    int dropped() const { return dropped_; }
    int queued() const { return queued_; }

private:
    EventQueue events_;
    SeededRng rng_;
    Config config_;
    swarm::core::Time linkDelay_;
    std::map<NodeAddress, std::unique_ptr<Router>> routers_;
    std::map<NodeAddress, std::set<NodeAddress>> links_;
    std::vector<std::string> trace_;
    std::uint32_t trails_ = 0;
    int delivered_ = 0;
    int dropped_ = 0;
    int queued_ = 0;
};

}  // namespace termite::core::test

#endif  // TERMITE_CORE_TEST_SUPPORT_H
