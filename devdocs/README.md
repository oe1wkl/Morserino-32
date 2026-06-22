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
├── consistency-audit/            ← 2026-06 UX/mode consistency audit
│   ├── mode-matrix.md            ·   per-mode comparison matrix
│   ├── todo-resolutions.md       ·   every TODO(audit) answered
│   ├── divergences.md            ·   ranked refactoring backlog
│   └── REFACTORING_PLAN.md       ·   phased plan (Phases A–G)
└── protocol-audit/               ← USB serial protocol audit
    ├── command-matrix.md         ·   one row per command, three-corner presence
    ├── conflicts.md              ·   three-way disagreements + live resolution status
    ├── PROTOCOL_SPEC.draft.md    ·   draft normative spec
    ├── utility-enhancements.md   ·   config-tool enhancement notes
    └── RESOLUTION_PLAN.md        ·   phased plan to close the conflicts
```

## Conventions

- **Group related developer docs into a subdirectory** (like the two audit folders above).
  Standing/operational docs (conventions, release runbooks, handoff) stay at the top level.
- Cross-reference other dev docs by their `devdocs/...` path so links survive moves.
- Release runbooks (`RELEASE_AUTOMATION_DESIGN.md`, `RUNNER_SETUP.md`) are referenced by name
  from `.github/workflows/release.yml` and `scripts/release/` — keep those names stable.
