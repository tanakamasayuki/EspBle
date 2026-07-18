# Changelog

## Unreleased

- Redesign HID around the EspUsbDevice/EspUsbHost API shape: `hidKeyboard()` / `hidMouse()` / `hidConsumerControl()` / `hidSystemControl()` / `hidGamepad()` compose one HOGP service, while `hidHost()` discovers and dispatches all supported report types.
- Add fixed cross-library Report IDs, keyboard layout-aware sending, mouse/consumer/system/gamepad helpers, per-report CCCD routing, generic Report Map parsing, shared-ownership Host listeners, composite peer coverage, and new HID examples.
- Establish the initial project, specification, and test structure.
- Add the first EspBle stack, legacy advertising, scanning, deferred scan-result callback, examples, and two-board peer test.
- Add central/peripheral connections with library connection IDs, disconnect, connection snapshots, and MTU exchange with `onMtuChanged()`.
- Add generic GATT server/client: known-UUID discovery, read, write with response, notify/indicate, subscription, and CCCD events.
- Add security: Just Works and static-passkey pairing with LE Secure Connections, bonding, encrypted/authenticated characteristic permissions, and bond management.
- Add the HID Keyboard Device profile: fixed 6KRO report protocol keyboard with HID/Device Information/Battery services, LED output reports, and battery level updates.
- Add the HID Keyboard Host profile: HID discovery, input report subscription, 256-bit usage snapshots, `onKeyboard()` press/release events, 19 EspUsbHost-compatible keyboard layouts, LED output writes, and battery reads.
- Add fixed-capacity event listeners (`add*Listener()` / `removeListener()`) to the HID Keyboard Host for ESP32KeyBridge adapter coexistence.
- Harden the connection lifecycle: lifecycle events evict droppable events instead of being lost, retired `BLEClient` objects are reaped without leaks or double frees, `end()` cancels in-flight connects and flushes stale scan results, and a second `begin()` with a different config fails instead of silently keeping the old one. Drop counters are exposed via `droppedEventCount()`.
- Replace byte-exact report-map matching with a minimal HID report descriptor parser: order-independent 6KRO keyboard detection, report-ID based input/output selection, and boot keyboards without report IDs. Rollover (phantom) reports are ignored and invalid-length input reports are counted via `invalidInputReportCount()`.
- Enforce HOGP security on the HID Keyboard Device: encrypted permissions on HID service attributes when security is enabled, and input reports are only notified to subscribed (and encrypted, when required) connections.
- Merge advertised service UUIDs into one Complete List AD structure per UUID size (CSS Part A 1.1).
- Switch keyboard layout conversion to EspUsbHost-compatible Unicode 4-plane tables with AltGr layers and character-pair Caps Lock handling; add a `unicode` field to keyboard events and fix the nl-NL and pt-BR tables.
- Make `setKeyboardLeds()` a non-blocking fire-and-forget write using Write Without Response.
- Add host unit tests (keymap conversion, report-map parser) and peer test suites for lifecycle stress, HID robustness, HID security, boot keyboards, and raw advertising payloads.
- Enforce the `connect()` timeout from `update()` with `ble_gap_conn_cancel()`: the bundled NimBLE `BLEClient::connect()` ignores its timeout argument and always waits its internal 30-second default.
- Add peer tests for asynchronous connect-timeout failure, exclusive central GATT operations, and silent peer loss detected via supervision timeout.
- Add a GitHub Actions workflow that compiles every example with the esp32s3 profile.
- Add bilingual (English/Japanese) READMEs for every example plus an examples index, expand the top-level and test READMEs with cross-links, and add `keywords.txt` for Arduino IDE syntax highlighting.
- Add a BLE primer (Classic vs BLE, no SPP, GAP/GATT/HID/security concepts) to the examples index.
- Add diagnostic examples: `Info/ScanDump` (full advertisement dump) and `Info/ConnectionInspector` (interactive connection/security/bond/counter inspector).
- Add examples for acknowledged delivery (`Gatt/IndicateServer` / `Gatt/IndicateClient`) and the passkey-input side of MITM pairing (`Security/StaticPasskeyClient`).
- Add `tools/version_matrix.py` and the manual `core-matrix.yml` / `board-matrix.yml` workflows that build the examples across arduino-esp32 core versions and boards in CI, generating `docs/COMPATIBILITY.<version>.md` / `docs/BOARDS.<version>.md`. Running these locally is discouraged because a sweep rewrites every sketch.yaml.
