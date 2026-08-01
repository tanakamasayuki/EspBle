# EspBle Documentation Guide

> 日本語版: [README.ja.md](README.ja.md)

The design documents are written in Japanese. This guide lists them and shows the fastest reading order to understand the project's status.

## Get oriented (recommended order for newcomers)

1. [../README.md](../README.md) — library overview, supported/unsupported chips, getting started
2. [STATUS.md](STATUS.md) — **where things stand**: verification, limitations, and remaining work for 1.0.0
3. [DECISIONS.ja.md](DECISIONS.ja.md) — the ledger of settled design decisions (the "why")
4. [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) / [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md) — specifications for the completed composite HID API redesign

**When in doubt, read STATUS then DECISIONS** — that gives you the current state and the reasoning behind it.

## Index by purpose

| What you want | Document |
|---|---|
| Progress so far and what's next | [STATUS.md](STATUS.md) |
| What the library is meant to be (requirements/scope) | [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) |
| Which features are done / planned / out of scope | [FEATURE_MATRIX.md](FEATURE_MATRIX.md) |
| Design philosophy and layering | [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) |
| Public API design principles | [API_DESIGN.ja.md](API_DESIGN.ja.md) |
| Terminology and naming rules | [TERMINOLOGY.ja.md](TERMINOLOGY.ja.md) |
| Settled decisions and their rationale | [DECISIONS.ja.md](DECISIONS.ja.md) |
| HID spec (Device / Host) | [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) / [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md) |
| BLE communication beginner guide: GAP / security / GATT / UUID / HID / BLE MIDI | [GUIDE_BLE_BASICS.md](GUIDE_BLE_BASICS.md) |
| Test strategy and coverage | [../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md) |
| Release checks | [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) |
| Board / core build matrices (CI-generated) | `BOARDS.<version>.md` / `COMPATIBILITY.<version>.md` |
| ESP32-P4 / ESP-Hosted setup, versions, and C6 update (Japanese) | [ESP_HOSTED_SETUP.ja.md](ESP_HOSTED_SETUP.ja.md) |
| Verified ESP32-P4 / ESP-Hosted limitations (Japanese) | [ESP_HOSTED_LIMITATIONS.ja.md](ESP_HOSTED_LIMITATIONS.ja.md) |
| Arduino Core issue draft for P4 Secure Connections | [ESP_HOSTED_SC_ISSUE.md](ESP_HOSTED_SC_ISSUE.md) |
| Usage examples | [../examples/README.md](../examples/README.md) |

## Document roles

- **Settled specification (authoritative)**: [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md), [DECISIONS.ja.md](DECISIONS.ja.md), and the SPEC files.
- **Status tracking**: [STATUS.md](STATUS.md) — progress, limitations, TODO; updated per work batch.
- **CI-generated (do not hand-edit)**: `BOARDS.<version>.md`, `COMPATIBILITY.<version>.md`, produced by `.github/workflows/board-matrix.yml` and `core-matrix.yml`.

## One-line status

The **BLE foundation and composite HID Device / Host are implemented and covered by peer/unit tests**. Further practical HID extensions and interoperability checks are in progress, with 1.0.0 planned as the first public release.
