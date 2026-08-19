# EspBle Documentation Guide

> 日本語版: [README.ja.md](README.ja.md)

The design documents are written in Japanese. This guide lists them and shows the fastest reading order to understand the project's status.

## Get oriented (recommended order for newcomers)

1. [../README.md](../README.md) — library overview, supported/unsupported chips, getting started
2. [STATUS.md](STATUS.md) — **where things stand**: verification, limitations, and remaining work for the next release
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
| Public API design principles | [API_DESIGN.md](API_DESIGN.md) |
| Terminology and naming rules | [TERMINOLOGY.ja.md](TERMINOLOGY.ja.md) |
| Settled decisions and their rationale | [DECISIONS.ja.md](DECISIONS.ja.md) |
| Writing and verifying a HID Report Descriptor | [GUIDE_HID_DESCRIPTORS.md](GUIDE_HID_DESCRIPTORS.md) |
| HID spec (Device / Host) | [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) / [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md) |
| BLE communication beginner guide: GAP / security / GATT / UUID / HID / BLE MIDI | [GUIDE_BLE_BASICS.md](GUIDE_BLE_BASICS.md) |
| Bluetooth Classic beginner guide: inquiry / radio settings / SPP / security / HID / A2DP / HFP (original ESP32 only) | [GUIDE_CLASSIC_BASICS.md](GUIDE_CLASSIC_BASICS.md) |
| Migrating from another library (`BLEDevice`, NimBLE-Arduino, `BluetoothSerial`) | [GUIDE_MIGRATION.md](GUIDE_MIGRATION.md) |
| EspBle in depth: execution model, capacities, backpressure, reconnection, dual-host internals, footprint, debugging | [GUIDE_ADVANCED.md](GUIDE_ADVANCED.md) |
| Choosing between BLE and Classic, and what differs where both exist | [CLASSIC_VS_BLE.md](CLASSIC_VS_BLE.md) |
| What Classic exposes, and what is verified, unverified or unimplemented (Japanese) | [CLASSIC_FEATURE_INVENTORY.ja.md](CLASSIC_FEATURE_INVENTORY.ja.md) |
| Test strategy and coverage | [../tests/TEST_PLAN.md](../tests/TEST_PLAN.md) |
| Release checks | [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) |
| Remaining gates for the next release (Japanese) | [PLAN_RELEASE_NEXT.ja.md](PLAN_RELEASE_NEXT.ja.md) |
| Board / core build matrices (CI-generated) | `BOARDS.<version>.md` / `COMPATIBILITY.<version>.md` |
| Original-ESP32 support: policy, upstream sources, verification plan (Japanese) | [PLAN_ESP32.ja.md](PLAN_ESP32.ja.md) |
| Original-ESP32 Classic implementation plan: distribution form and stages (Japanese) | [PLAN_ESP32_CLASSIC.ja.md](PLAN_ESP32_CLASSIC.ja.md) |
| Classic Audio (A2DP / AVRCP / HFP) expansion plan (Japanese) | [PLAN_ESP32_CLASSIC_AUDIO.ja.md](PLAN_ESP32_CLASSIC_AUDIO.ja.md) |
| Bundled hosts against other Arduino-ESP32 core versions: test plan (Japanese) | [PLAN_CORE_VERSION_MATRIX.ja.md](PLAN_CORE_VERSION_MATRIX.ja.md) |
| Classic host distribution re-evaluation: archive vs source vs hybrid (Japanese) | [PLAN_CLASSIC_HOST_DISTRIBUTION.ja.md](PLAN_CLASSIC_HOST_DISTRIBUTION.ja.md) |
| ESP32-P4 / ESP-Hosted support plan (Japanese) | [PLAN_ESP_HOSTED.ja.md](PLAN_ESP_HOSTED.ja.md) |
| Coexistence research and hardware validation record for NimBLE plus Classic (Japanese) | [TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md) |
| A2DP validation and fix request sent to PCMFlowBluetooth (Japanese) | [REQUEST_PCMFLOWBLUETOOTH_A2DP_VALIDATION.ja.md](REQUEST_PCMFLOWBLUETOOTH_A2DP_VALIDATION.ja.md) |
| Work plan for the 1.0.0 release (history, Japanese) | [PLAN_RELEASE_1_0_0.ja.md](PLAN_RELEASE_1_0_0.ja.md) |
| Original-ESP32 Classic handoff and remaining work (Japanese) | [HANDOFF_ESP32_CLASSIC.ja.md](HANDOFF_ESP32_CLASSIC.ja.md) |
| Rebuilding the Classic-only Bluedroid archive (Japanese) | [CLASSIC_HOST_BUILD.ja.md](CLASSIC_HOST_BUILD.ja.md) |
| ESP32-P4 / ESP-Hosted setup, versions, and C6 update (Japanese) | [ESP_HOSTED_SETUP.ja.md](ESP_HOSTED_SETUP.ja.md) |
| Verified ESP32-P4 / ESP-Hosted limitations (Japanese) | [ESP_HOSTED_LIMITATIONS.ja.md](ESP_HOSTED_LIMITATIONS.ja.md) |
| Arduino Core issue draft for P4 Secure Connections | [ESP_HOSTED_SC_ISSUE.md](ESP_HOSTED_SC_ISSUE.md) |
| Usage examples | [../examples/README.md](../examples/README.md) |

## Document roles

- **Settled specification (authoritative)**: [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md), [DECISIONS.ja.md](DECISIONS.ja.md), and the SPEC files.
- **Status tracking**: [STATUS.md](STATUS.md) — progress, limitations, TODO; updated per work batch.
- **CI-generated (do not hand-edit)**: `BOARDS.<version>.md`, `COMPATIBILITY.<version>.md`, produced by `.github/workflows/board-matrix.yml` and `core-matrix.yml`.

## One-line status

The **BLE foundation and composite HID Device / Host are released and covered by peer/unit tests**. Original-ESP32 Classic shipped in 1.3.0, with each feature's state written down in the [feature inventory](CLASSIC_FEATURE_INVENTORY.ja.md) (Japanese). Running BLE and Classic together is hardware-verified but stays experimental. The automated regressions all pass; what remains is interoperability with external devices.
