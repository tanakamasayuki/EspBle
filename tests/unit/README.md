# Unit Tests

> 日本語版: [README.ja.md](README.ja.md)

Pure C++/data conversion tests that run on the host with g++. No boards or serial ports are required.

```sh
uv run --env-file .env pytest unit/
```

## Suites

- `keymap`: verifies the HID usage → character conversion in `src/EspBleKeymap.h` (`espBleUsageToUnicode` / `espBleUsageToAscii`) against expected values derived from the primary sources (Windows layout data) for each layout. Pins down AltGr layer selection and fallback, character-pair Caps Lock handling, dead keys returning 0, and `ascii` = 0 for non-Latin-1 characters (EspUsbHost-compatible behavior).
- `report_map`: verifies the HID Report Map parser in `src/EspBleHidReportMap.h` — keyboards with reordered descriptor items, boot keyboards without a report ID, keyboards with an additional Consumer Control report, mouse-only descriptors, and truncated descriptors.
- `midi`: verifies the BLE MIDI packet codec in `src/EspBleMidi.h` — timestamp header/low-byte decoding, running status (with and without the timestamp byte), System Real-Time interleaving, System Exclusive within one packet and across two, malformed packets, the packet builder (single/multiple messages, timestamp-window rejection, overflow, round trip), and the multi-packet SysEx encoder (single-packet output, split-across-packets encode → parse round trip, and framing validation).
- `medical_float`: verifies the IEEE-11073 medical float codec in `src/EspBleMedicalFloat.h` — 32-bit FLOAT and 16-bit SFLOAT encode/decode round trips, exact little-endian byte layout, negative mantissas, and the reserved NaN / NRes / +-INFINITY values used by Health Thermometer, Blood Pressure, and Glucose.
- `cgm_crc`: verifies the CGM E2E-CRC codec in `src/EspBleCgmCrc.h` (CRC-16/MCRF4XX) — the documented check value 0x6F91 for "123456789", the empty-input initial value, append/verify round trips over a representative CGM Measurement, corruption detection, and rejection of values too short to hold a CRC.
