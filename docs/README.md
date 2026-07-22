# EspBle Documentation Guide

> 日本語版: [README.ja.md](README.ja.md)

The design documents are written in Japanese. This guide lists them and shows the fastest reading order to understand the project's status.

## Get oriented (recommended order for newcomers)

1. [../README.md](../README.md) — library overview, supported/unsupported chips, getting started
2. [STATUS.ja.md](STATUS.ja.md) — **where things stand**: verification, limitations, and remaining work for 1.0.0
3. [DECISIONS.ja.md](DECISIONS.ja.md) — the ledger of settled design decisions (the "why")
4. [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) / [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md) — specifications for the completed composite HID API redesign

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
| Pre-1.0.0 design-flaw audit and remediation plan | [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) |
| HID spec (Device / Host) | [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) / [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md) |
| Scan-to-GATT beginner guide (Japanese) | [GUIDE_SCAN_TO_COMM.ja.md](GUIDE_SCAN_TO_COMM.ja.md) |
| Test strategy and coverage | [../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md) |
| Release checks | [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) |
| Arduino-ESP32 upstream request draft (Japanese) | [UPSTREAM_REQUEST_ARDUINO_ESP32_DESCRIPTOR_CONTEXT.ja.md](UPSTREAM_REQUEST_ARDUINO_ESP32_DESCRIPTOR_CONTEXT.ja.md) |
| Board / core build matrices (CI-generated) | `BOARDS.<version>.md` / `COMPATIBILITY.<version>.md` |
| Usage examples | [../examples/README.md](../examples/README.md) |

## Document roles

- **Settled specification (authoritative)**: [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md), [DECISIONS.ja.md](DECISIONS.ja.md), and the SPEC files.
- **Status tracking**: [STATUS.ja.md](STATUS.ja.md) — progress, limitations, TODO; updated per work batch.
- **CI-generated (do not hand-edit)**: `BOARDS.<version>.md`, `COMPATIBILITY.<version>.md`, produced by `.github/workflows/board-matrix.yml` and `core-matrix.yml`.

## One-line status

The **BLE foundation and composite HID Device / Host are implemented and covered by peer/unit tests**. Further practical HID extensions and interoperability checks are in progress, with 1.0.0 planned as the first public release.
