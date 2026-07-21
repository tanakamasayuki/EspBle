# Changelog

## Unreleased

- Add connection-scoped GATT database enumeration, descriptor server definitions and client read/write operations, descriptor write events, per-operation timeouts, and real-device Write Without Response coverage.
- Add `EspBleAddressType` across scan, connection, and bond values plus direct asynchronous connection by address and type.
- Add standalone Battery, Device Information, Current Time, Heart Rate, Environmental Sensing, and Nordic UART Service compatible Server/Client examples, plus real-device coverage of their standard wire formats and notifications.
- Add BLE MIDI: an Arduino-independent packet codec (`EspBleMidi.h`: timestamp header/low-byte encoding, MIDI running status, System Real-Time interleaving, and multi-packet System Exclusive) with host unit tests, and `EspBleMidiDevice` / `EspBleMidiHost` profile helpers (`EspBleMidiProfile.h`) that follow the EspUsbDevice/EspUsbHost MIDI API. Includes Device/Host examples and two peer tests that validate the wire format and running-status decoding against a bundled-NimBLE peer.
- Add the standard Health Thermometer Service: Temperature Type reads and IEEE-11073 32-bit FLOAT Temperature Measurement indications, backed by a reusable medical FLOAT/SFLOAT codec (`EspBleMedicalFloat.h`) with host unit tests. Includes Server/Client examples and a peer test.
- Add the standard Blood Pressure Service: Blood Pressure Feature reads and IEEE-11073 16-bit SFLOAT systolic/diastolic/mean Measurement indications (reusing the medical-float codec). Includes Server/Client examples and a peer test.
- Add the standard Weight Scale Service: Weight Scale Feature reads and Weight Measurement indications carrying a uint16 weight at 0.005 kg resolution. Includes Server/Client examples and a peer test.
- Add multi-packet SysEx sending on both MIDI sides: `sendSysEx()` splits a framed message across BLE packets (`EspBleMidiSysExEncoder`) and transmits them one at a time from the send-completion event (`onSent` / `onCharacteristicWritten`), since BLE MIDI sends are single-in-flight. Both peer tests reassemble a 300-byte SysEx with an independent reassembler.
- Redesign HID around the EspUsbDevice/EspUsbHost API shape: `hidKeyboard()` / `hidMouse()` / `hidConsumerControl()` / `hidSystemControl()` / `hidGamepad()` / `hidVendor()` compose one HOGP service, while `hidHost()` discovers and dispatches all supported report types, including bidirectional Vendor Input / Output / Feature reports.
- Add fixed cross-library Report IDs, keyboard layout-aware sending, configurable mouse buttons, mouse/consumer/system/gamepad helpers, per-report CCCD routing, descriptor-driven Report Map/input parsing, wrap-safe shared-ownership Host listeners, disconnect state reset, composite peer coverage, and new HID examples.
- Add EspUsbDevice-compatible 29-byte NKRO Keyboard reports, `enableNkro()` / `releaseUsage()`, Host bitmap parsing, and real-device coverage for eight simultaneous keys, high usages, individual release, and LED output.
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
