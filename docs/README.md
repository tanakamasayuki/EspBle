# EspBle Documentation Guide

> 日本語版: [README.ja.md](README.ja.md)

The design documents are written in Japanese. This guide lists them and shows the fastest reading order to understand the project's status.

## Get oriented (recommended order for newcomers)

1. [../README.md](../README.md) — library overview, supported/unsupported chips, getting started
2. [STATUS.ja.md](STATUS.ja.md) — **where things stand**: implemented features, main limitations, next work (P0). This is the entry point.
3. [DECISIONS.ja.md](DECISIONS.ja.md) — the ledger of settled design decisions (the "why")
4. Work in progress → [HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md) — task list for the initial release's centerpiece (composite HID + API redesign)

**When in doubt, read STATUS then DECISIONS** — that gives you the current state and the reasoning behind it.

## Index by purpose

| What you want | Document |
|---|---|
| Progress so far and what's next | [STATUS.ja.md](STATUS.ja.md) |
| What the library is meant to be (requirements/scope) | [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) |
| Which features are done / planned / out of scope | [FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md) |
| Design philosophy and layering | [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) |
| Public API design principles | [API_DESIGN.ja.md](API_DESIGN.ja.md) |
| Terminology and naming rules | [TERMINOLOGY.ja.md](TERMINOLOGY.ja.md) |
| Settled decisions and their rationale | [DECISIONS.ja.md](DECISIONS.ja.md) |
| HID Keyboard spec (Device / Host) | [HID_KEYBOARD_DEVICE_SPEC.ja.md](HID_KEYBOARD_DEVICE_SPEC.ja.md) / [HID_KEYBOARD_HOST_SPEC.ja.md](HID_KEYBOARD_HOST_SPEC.ja.md) |
| The in-progress HID redesign plan | [HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md) |
| Overall development phases | [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) |
| Test strategy and coverage | [../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md) |
| Board / core build matrices (CI-generated) | `BOARDS.<version>.md` / `COMPATIBILITY.<version>.md` |
| Usage examples | [../examples/README.md](../examples/README.md) |

## Document roles

- **Settled specification (authoritative)**: [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md), [DECISIONS.ja.md](DECISIONS.ja.md), and the SPEC files.
- **Status tracking**: [STATUS.ja.md](STATUS.ja.md) — progress, limitations, TODO; updated per work batch.
- **Plans in progress (may contain unsettled items)**: [HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md), [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md).
- **CI-generated (do not hand-edit)**: `BOARDS.<version>.md`, `COMPATIBILITY.<version>.md`, produced by `.github/workflows/board-matrix.yml` and `core-matrix.yml`.

## One-line status (as of 2026-07-18)

Design-freeze phase toward the initial release (0.1.0). Decided to include the **HID API redesign (composite HID + EspUsbDevice/Host-style API)** in the first release; Phase 0 (design freeze) of [HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md) is done. Next is implementing Phase 1 (device backend consolidation) test-first. See STATUS for details.
