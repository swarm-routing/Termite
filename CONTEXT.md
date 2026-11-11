# CONTEXT.md — orientation and history

Background an agent or a new contributor needs that does not belong in
`AGENTS.md`: domain notes, current state, glossary, open questions. Read the
section a task needs, not the whole file.

## Current state

**Status: Phase 5** (swarm-routing/.github#51); `core/` is being written (#6), after the fidelity sheet (#4) and the anchor-miss pre-registration (#5). Termite exists today only as AntHocNet's `termite` comparator arm (GPL-2.0-only, `ns3/baselines/adaptive/`). In AntHocNet v2.5.0 it built, passed the delivery smoke (19.6 %) and **missed its anchor** (the trend points the right way; the CIs span zero), so it shipped as an infeasibility verdict. Its fidelity sheet was written from Roth's 2005 dissertation (chapter 4, Table 4.1; anchor from §4.4); the GLOBECOM 2003 paper and the SIDM 2005 copy were never read (swarm-routing/AntHocNet#587).

## Glossary

## Open questions

- Why did AntHocNet's arm miss its anchor: the arm (fidelity), the scenario (AntHocNet's anchor cell vs §4.4's setup) or power (seeds)? Pre-registered before code (AGENTS.md rule 12).
