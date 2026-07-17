# Unit Tests

> 日本語版: [README.ja.md](README.ja.md)

Pure C++/data conversion tests that run on the host with g++. No boards or serial ports are required.

```sh
uv run --env-file .env pytest unit/
```

## Suites

- `keymap`: verifies the HID usage → character conversion in `src/EspBleKeymap.h` (`espBleUsageToUnicode` / `espBleUsageToAscii`) against expected values derived from the primary sources (Windows layout data) for each layout. Pins down AltGr layer selection and fallback, character-pair Caps Lock handling, dead keys returning 0, and `ascii` = 0 for non-Latin-1 characters (EspUsbHost-compatible behavior).
- `report_map`: verifies the HID Report Map parser in `src/EspBleHidReportMap.h` — keyboards with reordered descriptor items, boot keyboards without a report ID, keyboards with an additional Consumer Control report, mouse-only descriptors, and truncated descriptors.
