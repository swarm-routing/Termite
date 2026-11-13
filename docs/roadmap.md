# Roadmap

The plan in one page. The live plan is this page plus the `release:` labels
below; issue state is always truer than a checked-in diagram. The release
ladder was adopted by the maintainer on 2026-11-11 ("adopt the release ladder
and replace the phase-targets rule"), after the model of
[AntHocNet's roadmap](https://github.com/swarm-routing/AntHocNet/blob/main/docs/roadmap.md).
Termite was a Phase 5 target of the org ROADMAP; it now has versions instead.

## How this plan is tracked

Each issue a release gates carries a **`release:` label**, so every row of
this plan is a live query rather than a diagram that drifts:

| Release | Board |
|---|---|
| v0.2.0 | [`release:v0.2.0`](https://github.com/swarm-routing/Termite/issues?q=is%3Aopen+label%3A%22release%3Av0.2.0%22) |
| v0.3.0 | [`release:v0.3.0`](https://github.com/swarm-routing/Termite/issues?q=is%3Aopen+label%3A%22release%3Av0.3.0%22) |
| v1.0.0 | [`release:v1.0.0`](https://github.com/swarm-routing/Termite/issues?q=is%3Aopen+label%3A%22release%3Av1.0.0%22) |
| all epics | [`epic`](https://github.com/swarm-routing/Termite/issues?q=is%3Aopen+label%3Aepic) |

Only issues a release actually gates are labelled. Majors are reserved for
breaking changes; v1.0.0 is the stability promise (wire format and `core/`
API frozen, paper-faithful with the deviations documented, the three gates
passed, green on the supported ns-3 release).

## Release ladder

```mermaid
flowchart LR
    V01["<b>v0.1.0</b><br/>first release<br/>(ungated)"]
    V02["<b>v0.2.0</b><br/>gated"]
    V03["<b>v0.3.0</b><br/>reviewed"]
    V10["<b>v1.0.0</b><br/>wire + core/ API<br/>frozen"]

    V01 --> V02 --> V03 --> V10

    style V01 fill:#e2f0ed,stroke:#0f7f70,stroke-width:2px
    style V02 fill:#fff8e6,stroke:#c48f00,stroke-dasharray:4 3
    style V03 fill:#fff8e6,stroke:#c48f00,stroke-dasharray:4 3
    style V10 fill:#f6dede,stroke:#c0392b,stroke-width:2px
```

Green is shipped, amber is planned. v0.1.0 shipped on 2026-11-11
([release v0.1.0](https://github.com/swarm-routing/Termite/releases/tag/v0.1.0)).

## What blocks what

Solid arrows are hard dependencies; dashed are "informs". Cross-repository
dependencies are named `swarm-routing/<repo>#N`.

```mermaid
flowchart TB
    I95["#95 data header counted<br/>as control by routing-bench"]
    I59["#59 anchor-miss cells<br/>(pre-registration #5)"]
    I6["#6 core/ + ns-3 module<br/>through the three gates"]
    I31["#31 pipeline + anchor"]
    I4["#4 fidelity sheet<br/>last criterion"]
    I22["#22 review against<br/>GLOBECOM 2003, SIDM 2006, ABC"]
    I42["#42 perf follow-ups"]
    I43["#43 hot-path pheromone reads"]
    I80["#80 v1.0.0: wire format<br/>and core/ API frozen"]

    SC47["ns3-swarm-core#47"]
    NT["ns3-tools v0.2.0<br/>campaigns and CLI"]
    SC10["ns3-swarm-core v1.0.0"]
    BM["benchmarks<br/>(run format)"]

    I95 --> I31
    I95 --> I59
    NT --> I31
    BM --> I31
    I59 --> I31
    I31 --> I6
    I6 --> I42
    SC47 -.-> I43
    I22 -.-> I4
    I6 --> I80
    I4 --> I80
    I22 --> I80
    I42 --> I80
    I43 --> I80
    SC10 --> I80

    style I6 fill:#e8e6f8,stroke:#5b4fc4,stroke-width:2px
```

**The sequencing rule worth memorising:** the anchor miss is explained before
the anchor is claimed. AntHocNet's arm passed the delivery smoke but missed its
anchor in v2.5.0 (trend right, confidence intervals span zero, danieljoppi/AntHocNet#587);
the pre-registration on [#5](https://github.com/swarm-routing/Termite/issues/5) (closed 2026-11-11) chose the cells, and [#59](https://github.com/swarm-routing/Termite/issues/59) runs them and applies the decision rule.

The router observer is no longer in the picture: [#41](https://github.com/swarm-routing/Termite/issues/41) (the core and the ns-3 half) closed on 2026-11-11 ([`observer.md`](observer.md)). [#95](https://github.com/swarm-routing/Termite/issues/95) is: the data header sits in front of the UDP header, so routing-bench would count every data hop as control, and it is fixed before #31 or #59 report overhead.

## What each release must show to ship

| Release | Exit criteria |
|---|---|
| **v0.1.0** | **Shipped 2026-11-11, ungated.** `core/` on swarm-core's primitives (the ch. 4 data path) and `docs/` (the fidelity sheet, [#4](https://github.com/swarm-routing/Termite/issues/4)) as they stood at the release commit. The `termite` contrib module was not in the release ([#37](https://github.com/swarm-routing/Termite/issues/37) closed later the same day), and the anchor was pending. |
| **v0.2.0** | **Gated.** The module is green on `debug` and `sanitized` with the determinism gate; the router observer is adopted; the protocol has its own benchmark pipeline; the anchor has a verdict (PASS, or a written infeasibility verdict); the fidelity open items are settled. Done: the router observer ([#41](https://github.com/swarm-routing/Termite/issues/41), closed 2026-11-11). Open:<br/>• [#59](https://github.com/swarm-routing/Termite/issues/59): run the pre-registered anchor-miss cells from [#5](https://github.com/swarm-routing/Termite/issues/5) and apply its decision rule.<br/>• [#6](https://github.com/swarm-routing/Termite/issues/6): the protocol issue's remaining acceptance criteria (gates recorded in `benchmarks`; AntHocNet's arm removed with the byte-identical check).<br/>• [#31](https://github.com/swarm-routing/Termite/issues/31): paired smoke and anchor campaigns against the base, on the org reusable cell workflow (needs ns3-tools v0.2.0). |
| **v0.3.0** | **Reviewed.** The paper-review issues, the performance follow-ups and default-off A/Bs.<br/>• [#4](https://github.com/swarm-routing/Termite/issues/4): the fidelity sheet's last criterion.<br/>• [#22](https://github.com/swarm-routing/Termite/issues/22): review the sheet against GLOBECOM 2003, SIDM 2006 and ABC once they are in hand.<br/>• [#42](https://github.com/swarm-routing/Termite/issues/42): zero-copy overhear, cached ipv4, one-pass decay.<br/>• [#43](https://github.com/swarm-routing/Termite/issues/43): hot-path pheromone reads off `entries()` (swarm-routing/ns3-swarm-core#47). |
| **v1.0.0** | **Stable.** Wire format and `core/` API frozen; a released run recorded in the `benchmarks` global suite.<br/>• [#80](https://github.com/swarm-routing/Termite/issues/80): freeze the wire format and the `core/` API, with the gates passed (after v0.2.0, v0.3.0 and ns3-swarm-core v1.0.0). |

Not gating, so not labelled: the epic [#3](https://github.com/swarm-routing/Termite/issues/3) and the handoff [#7](https://github.com/swarm-routing/Termite/issues/7). [#95](https://github.com/swarm-routing/Termite/issues/95) carries no `release:` label either; it is drawn above because the overhead numbers of #31 and #59 wait for it.

## Infrastructure track

CI, org-template and settings work rides along with whichever release is next
and never gates one, so it carries no `release:` label and is never an exit
criterion. No item is scheduled yet.

## Deliberate non-goals

- **A v1.0.0 before the gates.** v1.0.0 promises a frozen wire format and
  `core/` API; it waits for v0.2.0 (anchor verdict) and v0.3.0 (paper review).
- **Mechanisms beyond the papers by default.** Any addition ships as a
  default-off mechanism with its own A/B on identical seeds, as in AntHocNet.
- **Nothing is deferred past 1.0 yet.** Work found later is filed and labelled
  into a release only when that release depends on it.

## See also

- The org [ROADMAP](https://github.com/swarm-routing/.github/blob/main/ROADMAP.md): the cross-repository release train.
- [CONTRIBUTING](https://github.com/swarm-routing/.github/blob/main/CONTRIBUTING.md): labels and issue shape.
- [AntHocNet's roadmap](https://github.com/swarm-routing/AntHocNet/blob/main/docs/roadmap.md): the model for this page.
- [`fidelity.md`](fidelity.md) and [`observer.md`](observer.md).
