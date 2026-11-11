// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)
// SPDX-License-Identifier: GPL-2.0-only
//
// Termite as an arm of swarm-routing/ns3-tools' comparison harness
// (routing-bench, AntHocNet ADR-0027): compiled only when routing-bench is
// installed beside this module (CMakeLists.txt). routing-compare finds this
// file and calls RegisterArms_termite() at start-up; the arm's name is
// `termite`, the token AntHocNet's `adaptive` arm used, so v2.5.0's command
// lines run the new module unchanged. The two never share a build: a
// duplicate name aborts.
#include "ns3/routing-bench-arm.h"
#include "ns3/termite-helper.h"

namespace ns3 {
namespace termite {
namespace {

class TermiteArm : public bench::Arm {
public:
    std::string Name() const override { return "termite"; }
    void InstallRouting(InternetStackHelper& internet, bench::RunContext&) override {
        internet.SetRoutingHelper(m_helper);
    }
    int64_t AssignStreams(bench::RunContext& run, int64_t stream) override {
        return m_helper.AssignStreams(run.nodes, stream);
    }
    // InstallRouting only names the helper; the monitors TermiteHelper::Install
    // would add (a WifiLinkMonitor per wifi node) are added here instead.
    void AfterStack(bench::RunContext& run) override { SwarmMonitorHelper().Install(run.nodes); }

private:
    TermiteHelper m_helper;
};

Ptr<bench::Arm> MakeTermiteArm() {
    return Create<TermiteArm>();
}

}  // namespace
}  // namespace termite

namespace bench {

void RegisterArms_termite();

void RegisterArms_termite() {
    RegisterArm("termite",
                "Termite, standalone on swarm-core (Roth & Wicker 2003; Roth 2005 ch. 4)",
                &termite::MakeTermiteArm);
}

}  // namespace bench
}  // namespace ns3
