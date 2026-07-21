# Peer Tests

> 日本語版: [README.ja.md](README.ja.md)

Automated Bluetooth LE tests using two ESP32-S3 boards. The boards only need their independent serial/power connections; no signal wiring is required between them.

```sh
uv run --env-file .env pytest peer/
```

The profiles and environment variables reuse the existing EspUsbHost/EspUsbDevice setup:

| pytest side | profile | environment variable |
|---|---|---|
| parent | `s3_peer_host` | `TEST_SERIAL_PORT_S3_PEER_HOST` |
| second peer | `s3_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE` |

The profile names do not describe BLE roles. Sketches are flashed to and run on both boards, and both serial ports are observed from pytest. The parent-side sketch is fixed as central and the `peer_device/` sketch as peripheral; the roles are never swapped.

## Test suites

- `stack_smoke`: uses only the bundled NimBLE backend BLE API to validate advertising/scan, connection, GATT read/write, both serial ports, and the two-board fixture.
- `advertise_scan`: EspBle advertising on the peer, EspBle scanner on the parent — name, 128-bit service UUID, manufacturer data, and `update()`-context delivery of scan results.
- `lifecycle_stress`: connection-lifecycle regressions — the Disconnected event and HID host slot release survive a notification flood while `update()` is paused (drops observable via `droppedEventCount()`); repeated connect / HID discover / disconnect does not leak the `BLEClient` or its service tree; `end()`/`begin()` survive an in-flight GATT read; `connect()` to an unreachable peer fails asynchronously near the requested timeout; a second GATT operation is rejected synchronously while one is in flight; and a silently vanished peer (radio killed without a Link Layer terminate) is detected via supervision timeout.
- `hid_robustness`: HID host/device robustness — no input reports to unsubscribed peers, rollover (phantom) reports are not misread as all-release, the disconnect all-release event survives a full event queue, `disconnect()` of the same connection is rejected during HID discovery, and a second `begin()` with a different config fails.
- `hid_security`: a security-enabled HID keyboard device rejects report-map reads, HID discovery, and input-report delivery over an unencrypted link.
- `hid_boot_keyboard`: a generic GATT server emulates a boot keyboard without a report ID (no Report Reference descriptor) — discovery and input work, and input reports with a length other than 8 leave the key state unchanged and are counted by `invalidInputReportCount()`.
- `advertise_payload`: the parent uses the bundled API's passive scan to capture the raw advertisement payload and asserts that multiple 16-bit service UUIDs are merged into a single Complete List AD structure with no duplicated AD types.
- `battery_service`: validates standalone Battery Service one-byte reads, CCCD subscription, notifications, send completion, and unsubscribe.
- `device_information`: validates standalone Device Information string reads and the standard 7-byte little-endian PnP ID wire format.
- `current_time`: validates standalone Current Time 10-byte decoding, CCCD subscription, notifications, send completion, and unsubscribe.
- `heart_rate`: validates Body Sensor Location reads and flags-driven 16-bit heart rate, Energy Expended, and RR-Interval notification decoding.
- `environmental_sensing`: validates signed Temperature, scaled Humidity and 32-bit Pressure reads, plus Temperature notification and unsubscribe.
- `midi_device`: a bundled-NimBLE central subscribes to the `EspBleMidiDevice` I/O characteristic and asserts the exact BLE MIDI wire bytes (header + timestamp + Note On/Off), the empty read, a multi-packet SysEx reassembled by an independent reassembler, and that a raw Control Change written by the central is decoded by the device.
- `midi_host`: a bundled-NimBLE peripheral notifies a running-status BLE MIDI packet and `EspBleMidiHost` decodes it into two messages with correct timestamps; the host's outgoing Note On and a multi-packet SysEx reach the peripheral.
- `health_thermometer`: the standard Health Thermometer server indicates an IEEE-11073 32-bit FLOAT Temperature Measurement; the client reads Temperature Type, subscribes to indications, and decodes 37.5 °C exactly.
- `blood_pressure`: the standard Blood Pressure server indicates a Measurement with systolic/diastolic/mean as IEEE-11073 16-bit SFLOATs; the client reads the Feature field and decodes 120/80/93 mmHg exactly.
- `weight_scale`: the standard Weight Scale server indicates a Measurement carrying a uint16 weight at 0.005 kg resolution; the client reads the Feature field and decodes 70.000 kg exactly.
- `body_composition`: the standard Body Composition server indicates a Measurement carrying uint16 flags, the mandatory Body Fat Percentage (0.1 %/LSB), and the optional Weight field (bit 10); the client reads the Feature field and decodes 27.5 % body fat and 70.000 kg exactly.
- `location_navigation`: the standard Location and Navigation server notifies a Location and Speed value carrying uint16 flags, Instantaneous Speed (1/100 m/s), and a sint32 latitude/longitude (1e-7 deg); the client reads LN Feature and decodes 5.00 m/s and the Tokyo coordinates exactly.
- `user_data`: the standard User Data Service exposes a read/write Age, a writable First Name, and a read/write/notify Database Change Increment. The client writes First Name and a new Age, the server receives each write in `onWritten` and bumps + notifies the increment, and the client sees it climb to 2 and re-reads Age as the written 42. Validates the client-write → onWritten → notify path.
- `alert_notification`: the standard Alert Notification Service exposes a readable Supported New Alert Category bitmask (Email + SMS/MMS, 0x0022), a notifiable New Alert, and a writable Alert Notification Control Point. The client reads the bitmask, subscribes to New Alert, and writes "Notify New Alert Immediately" for Email; the server notifies a New Alert (category 1, count 3, text "Bob") which the client decodes. Validates the Control Point write → notify path.
- `immediate_alert`: the standard Immediate Alert Service (the Find Me target role) exposes Alert Level (0x2A06) as a Write Without Response uint8. The client writes High Alert (2) then No Alert (0) without a response, and the server observes each write in `onWritten` from loop context. Validates the write-without-response-only standard-service path.
- `phone_alert_status`: the standard Phone Alert Status Service exposes read/notify Alert Status and Ringer Setting plus a Write Without Response Ringer Control Point. The client reads Alert Status (0x01), subscribes to Ringer Setting, reads the initial Normal setting, and drives the Control Point to Set Silent Mode (Ringer Setting notifies 0) and Cancel Silent Mode (notifies 1). Validates the Control Point write → server state change → notify path.
- `proximity`: the Proximity profile hosts the Link Loss Service (0x1803, read/write Alert Level) and the Tx Power Service (0x1804, read-only signed-int8 Tx Power Level) on one server. The client reads the Tx Power Level (-8 dBm), reads the initial Alert Level (0), writes High Alert (2) with response, and re-reads it as 2. Validates hosting two services on one server and signed-int8 reads.
- `cycling_speed_cadence`: the standard CSC server notifies a multi-field Measurement (cumulative wheel/crank revolutions and event times); the client reads Sensor Location, subscribes, and decodes every field.
- `running_speed_cadence`: the standard RSC server notifies a mixed-width Measurement (uint16 speed, uint8 cadence, optional uint16 stride length and uint32 total distance); the client reads Sensor Location, subscribes, and decodes every present field.
- `cycling_power`: the standard Cycling Power server notifies a Measurement with 16-bit flags and a signed 16-bit instantaneous power; the client reads Sensor Location, subscribes, and decodes a negative power exactly.
- `pulse_oximeter`: the standard Pulse Oximeter server indicates a Spot-Check Measurement whose SpO2 and pulse rate are IEEE-11073 16-bit SFLOATs; the client reads PLX Features and decodes 98 % / 60 bpm exactly.
- `glucose`: exercises the Record Access Control Point procedure — the client writes "Report Stored Records (all)"; the server notifies one Glucose Measurement (sequence, base time, SFLOAT concentration) and then indicates the RACP response, sequenced from `onSent`. Validates the write → notify → indicate choreography.
- `connect_disconnect`, `gatt_read_write`, `notify_indicate`, `mtu`, `security_bond`, `security_passkey`, `hid_keyboard_device`, `hid_keyboard_host`, `ble_keybridge_keyboard`: the per-feature suites listed in the [test plan](../TEST_PLAN.ja.md).
