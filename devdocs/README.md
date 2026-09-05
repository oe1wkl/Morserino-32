# devdocs — developer & maintainer documentation

Everything here is for **developers and maintainers**, not end users. User-facing
documentation (manuals, FAQ, assembly/hardware notes) lives under `Documentation/`.
The one deliberate boundary case is the **serial-protocol description**
(`Documentation/Protocol Description/`), which is kept under `Documentation/` because it
is useful to advanced users too. (This policy is also recorded in `CLAUDE.md` §7.)

## Layout

```
devdocs/
├── README.md                     ← this file
├── UX_CONVENTIONS.md             ← standing UX/control-grammar conventions (the rulebook)
├── HANDOFF.md                    ← session handoff notes
├── RELEASING.md                  ← release quick reference
├── RELEASE_AUTOMATION_DESIGN.md  ← release pipeline design  ─┐ referenced by
├── RUNNER_SETUP.md               ← self-hosted runner setup  ─┘ .github/workflows + scripts/release
├── audio-accessibility/          ← Accessibility Edition: spoken menus/prefs (Pocket only)
│   ├── HANDOFF.md                ·   START HERE — state, architecture, build/flash, gotchas
│   ├── IMPLEMENTATION_PLAN.md    ·   phased implementation plan + maintainer decisions
│   ├── FEASIBILITY_REPORT.md     ·   original flash/memory feasibility analysis
│   └── PRODUCT_PLAN.md           ·   plan for shipping it as a published firmware
├── ble-serial/                   ← M32 serial protocol over BLE (Nordic UART Service)
│   ├── PLAN.md                   ·   START HERE — implementation plan, decisions D1–D19
│   ├── DESIGN.md                 ·   contracts, deliberate exemptions, measured results
│   ├── ACCESS_CONTROL.md         ·   consent on the device: threat model, decisions, a11y
│   └── *.py, *.cpp               ·   host-side protocol tests + ring-buffer unit test
├── cn3_paddle/                   ← CN3 touch connector → mechanical paddle (Pocket only)
│   └── CN3_MECHANICAL_PADDLE_BRIEFING.md ·   pinout, feasibility, implementation notes
├── consistency-audit/            ← 2026-06 UX/mode consistency audit
│   ├── mode-matrix.md            ·   per-mode comparison matrix
│   ├── todo-resolutions.md       ·   every TODO(audit) answered
│   ├── divergences.md            ·   ranked refactoring backlog
│   └── REFACTORING_PLAN.md       ·   phased plan (Phases A–G)
├── cw-timing-audit/              ← 2026-07 CW element/gap timing audit
│   └── FINDINGS.md               ·   measurements, root causes, applied fixes, open item
├── device-aware-manual-links/    ← brief: link the right manual from the tools
│   └── HANDOFF.md                ·   installer + config tool, what each can detect
├── grid-games/                   ← Trailblazer + Fox Hunt grid-maze games (TFT only)
│   └── CONCEPT.md                ·   concept, scoring, multiplayer race, build steps
├── installer/                    ← unified web installer (V9)
│   └── PLAN.md                   ·   design + migration plan (2 installers → 1)
├── ios-app/                      ← iPhone config app: the web tool carried over BLE
│   └── DESIGN.md                 ·   why a native shell, the transport seam, firmware contracts
├── manual-variants/              ← V9 manual: one tagged source → 3 variants
│   ├── SURVEY.md                 ·   source survey, decisions, pandoc traps
│   ├── inventory-ambiguous.md    ·   what was NOT tagged, and the open questions
│   ├── inventory-*.md            ·   menu terms / images / tables / morse (generated)
│   └── check_*.py, *_terms.py    ·   build-identity + EN↔DE parallelism checks
├── memory-chain/                 ← Memory Chain game, CW "I packed my suitcase" (TFT only)
│   └── CONCEPT.md                ·   settled design decisions, both game modes
├── practice-stats/               ← practice statistics logging + viewers (Pocket only)
│   └── README.md                 ·   /stats.jsonl format, WiFi page, config-tool tab
├── protocol-audit/               ← USB serial protocol audit, and what 1.4 added
│   ├── command-matrix.md         ·   one row per command, three-corner presence
│   ├── conflicts.md              ·   three-way disagreements + live resolution status
│   ├── PROTOCOL_SPEC.draft.md    ·   draft normative spec
│   ├── utility-enhancements.md   ·   config-tool enhancement notes
│   ├── RESOLUTION_PLAN.md        ·   phased plan to close the conflicts
│   └── PROTOCOL_1.4_DESIGN.md    ·   the bulk preference read, sized and decided
├── qso-bot/                      ← QSO Bot behaviour review + improvement backlog
│   └── IMPROVEMENT_PLAN.md       ·   already-implemented vs. genuine remaining gaps
└── ultimatic/                    ← Ultimatic keyer mode vs. the 1955 original
    ├── FINDINGS.md               ·   the QST source, the point-6 bug, the fix
    ├── FOLLOWUP_CURTIS_B.md      ·   Ultimatic vs. the Curtis-B timing (decision pending)
    └── keyer_sim.cpp             ·   host-side keyer simulator; `scan` runs the follow-up
```

## Conventions

- **Group related developer docs into a subdirectory** (like the two audit folders above).
  Standing/operational docs (conventions, release runbooks, handoff) stay at the top level.
- Cross-reference other dev docs by their `devdocs/...` path so links survive moves.
- Release runbooks (`RELEASE_AUTOMATION_DESIGN.md`, `RUNNER_SETUP.md`) are referenced by name
  from `.github/workflows/release.yml` and `scripts/release/` — keep those names stable.
