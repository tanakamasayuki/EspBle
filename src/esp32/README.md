# Custom Classic Bluedroid host

`libespble_bluedroid_classic.a` is built from ESP-IDF v5.5.5 with its build
configuration disabling the controller and BLE, and with Classic SPP, HID Device,
HID Host, A2DP
Sink/Source and AVRCP CT/TG (external codec), and HFP Client/Audio Gateway
(Voice over HCI with an external codec) enabled. All symbols defined by the archive are prefixed with `espble_bd_` so
Arduino-ESP32's built-in Bluedroid host cannot satisfy or collide with them.
Undefined platform dependencies such as FreeRTOS, NVS and logging keep their
original names and are supplied by Arduino-ESP32.

"BLE disabled" describes the Kconfig selection. ESP-IDF can still place unused
BLE-named objects in the static archive; a Classic-only final link does not select
them. The artifact's complete provenance, hashes and license inventory are in
[`MANIFEST.json`](MANIFEST.json), [`NOTICE`](NOTICE) and [`LICENSES/`](LICENSES/).

[`include/`](include/) holds the Bluedroid API headers the archive was built
against, copied byte-identically from the same ESP-IDF by
`tools/vendor_classic_contract.py`. The Classic sources include these instead of
the core's copies, so the declarations and struct layouts always match what the
archive baked in, whatever Arduino-ESP32 version the sketch compiles against.
Do not edit them; regenerating the archive refreshes them together with it, and
`tools/verify_classic_archive.py` checks the pair.

To regenerate it, install and export ESP-IDF v5.5.5, then run:

```sh
tools/build_classic_bluedroid_host.sh
```

The output is ABI-bound to ESP-IDF v5.5.5 and xtensa-esp32 GCC 14.2.0. Because
the compile-time contract ships in `include/`, the archive's remaining coupling
to the core is a small set of stable platform APIs (FreeRTOS, NVS, esp_timer,
heap, logging); the core-version range this actually supports is measured, not
assumed — see the Japanese
[core-version test plan](../../docs/PLAN_CORE_VERSION_MATRIX.ja.md). The build
script rejects a different IDF tag, a dirty IDF checkout, or a different
compiler instead of silently producing an incompatible library. It also checks the required HCI, SPP, HID, A2DP, AVRCP and HFP symbols and prints the archive
size, global-symbol count and SHA-256 digest.

EspBle intentionally uses Arduino's mixed-library layout: the original ESP32's
NimBLE host is compiled from bundled source, while this ESP-IDF component is
distributed as a precompiled archive because it depends on an ESP-IDF component
build and post-build symbol namespacing.

The complete clean-room setup, reproducibility check, configuration and update
procedure are documented in
[`docs/CLASSIC_HOST_BUILD.ja.md`](../../docs/CLASSIC_HOST_BUILD.ja.md).
