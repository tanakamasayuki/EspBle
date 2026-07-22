# Changelog

## Unreleased

Initial feature set for the first release. Everything below is implemented and
verified with host unit tests and two-board (ESP32-S3) peer tests.

### Core stack & lifecycle

- Central / Peripheral and GATT client / server under one role-neutral `EspBle`
  owner, with library connection IDs, value-type connection snapshots, and
  explicit event delivery from `ble.update()` on the caller's loop task.
- Resilient lifecycle: retired `BLEClient` objects are reaped without leaks or
  double frees, event-queue overflow is bounded by drop counters
  (`droppedEventCount()`) with lifecycle/completion events kept over
  notifications, a second `begin()` with a different config fails instead of
  silently keeping the old one, and `end()` cancels in-flight connects and
  flushes stale scan results.

### GAP — advertising, scanning, connections

- Legacy advertising (name / service UUID / manufacturer data), connectable and
  non-connectable (optionally non-scannable) beacon/broadcaster modes, and
  advertising-interval control.
- Active / passive scanning with value-type scan results; advertising/scan
  Service Data (AD 0x16); Apple iBeacon build/parse via a backend-independent
  codec (`EspBleIBeacon.h`).
- Address privacy via `EspBleConfig::ownAddressType` (public / random static /
  resolvable private address).
- Connect by scan result or by address + type; multiple simultaneous Central
  connections (capped by the bundled NimBLE controller — 3 on esp32s3) with
  auto-reconnect (`setAutoReconnect`).
- Connection-parameter and LE PHY (2M) updates, and MTU exchange tracked for
  both roles via the global GAP event listener (`onMtuChanged()`).
- `EspBleConnection::disconnectReason` (backend/HCI reason code) on
  `onDisconnected()`.

### GATT

- Generic server: arbitrary services / characteristics / descriptors,
  encrypted/authenticated permissions, binary-safe values, and descriptor write
  events.
- Generic client: connection-scoped database enumeration and known-UUID
  discovery, read and write (with / without response), descriptor read/write,
  per-operation timeouts, handle-based overloads to target same-UUID
  characteristics, and automatic operation queuing (no "operation already in
  progress" failures).
- Notify / indicate with subscription management; persistent subscriptions
  restored automatically on reconnect (`EspBleConfig::persistentSubscriptions`,
  default on); GATT Service Changed indication (`notifyServicesChanged()`).

### Standard GATT services (examples + peer tests)

- Common: Battery, Device Information, Current Time, Heart Rate, Environmental
  Sensing, Nordic UART Service (NUS).
- Health & body: Health Thermometer, Blood Pressure, Weight Scale, Body
  Composition, Pulse Oximeter, Glucose (Record Access Control Point), Continuous
  Glucose Monitoring (End-to-End CRC).
- Fitness: Cycling Speed and Cadence, Running Speed and Cadence, Cycling Power,
  Fitness Machine (FTMS, including the interactive Control Point), Location and
  Navigation.
- Alerts & proximity: Alert Notification, Immediate Alert (Find Me target),
  Phone Alert Status, Proximity (Link Loss + Tx Power).
- Management: Reference Time Update, User Data, Bond Management.
- Reusable backend-independent codecs with host unit tests: IEEE-11073 medical
  FLOAT / SFLOAT (`EspBleMedicalFloat.h`) and CGM E2E-CRC (`EspBleCgmCrc.h`).

### HID over GATT (HOGP)

- Device: composable keyboard (6KRO / NKRO), mouse, consumer control, system
  control, gamepad, and vendor (input / output / feature) profiles in one HID
  service; arbitrary Report Descriptor via `ble.hidCustom()`; opt-in keyboard
  Boot Protocol.
- Host: one `hidHost()` discovers and dispatches every supported report type,
  with layout-aware keyboard decoding to Unicode / ASCII (19 layouts) and
  wrap-safe shared-ownership listeners.

### BLE MIDI

- Backend-independent packet codec (`EspBleMidi.h`: timestamps, running status,
  System Real-Time interleaving, multi-packet System Exclusive) with host unit
  tests, plus `EspBleMidiDevice` / `EspBleMidiHost` helpers that mirror the
  EspUsbDevice / EspUsbHost MIDI API.

### Security

- Just Works and passkey pairing (static passkey plus interactive runtime entry
  and Numeric Comparison) over LE Secure Connections, bonding, and
  encrypted / authenticated characteristic permissions.

### Tooling & docs

- A GitHub Actions workflow that compiles every example for the esp32s3 profile,
  manual core-/board-matrix workflows, bilingual (English / Japanese) example
  READMEs with a categorized index, host unit tests, and two-board peer test
  suites.
