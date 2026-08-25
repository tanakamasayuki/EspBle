# BLE or Bluetooth Classic

> 日本語版: [CLASSIC_VS_BLE.ja.md](CLASSIC_VS_BLE.ja.md)

EspBle offers both BLE (`EspBle`) and Bluetooth Classic (`EspBleClassic`). Some
features, HID among them, exist on both sides, so this document covers which one
to pick and what differs where both exist. For the concepts and API boundaries of
Classic see the [Classic beginner guide](GUIDE_CLASSIC_BASICS.md); for
per-API status see the [Feature Matrix](FEATURE_MATRIX.md), and for exactly what
Classic exposes see the
[Classic feature inventory](CLASSIC_FEATURE_INVENTORY.ja.md) (Japanese).

## 1. The short answer

**For anything new, BLE is the default.** It works on every ESP32 SoC, draws less
current, and phones, tablets and PCs from around 2015 onwards accept BLE HID
(HOGP).

Pick Classic only when one of these applies:

| Why Classic is needed | Examples |
|---|---|
| The peer does not accept BLE | Older consoles with BR/EDR only, old PCs, industrial equipment |
| A serial port profile (SPP) is required | A virtual COM port on a PC or Android, equipment with an existing serial protocol, instruments |
| Audio | A2DP (music) and HFP (calls); BLE has no standard audio path in this library |
| The peer only accepts Classic HID | Being recognised as a BR/EDR HID gamepad, keyboard or mouse |

**Classic works on the original ESP32 only.** ESP32-S3/C3/C6/H2/P4 have no
BR/EDR radio, so BLE is the only choice there. Classic ships without a support or
compatibility guarantee, and each feature's state — hardware-verified, unverified
or unimplemented — is documented rather than promised.

Conversely, **if the peer accepts BLE there is almost no reason to choose
Classic**: it is limited to the original ESP32, draws more current, and EspBle
bundles a separately built host for it, which costs extra flash and heap.

## 2. Classic has three real uses

What actually gets used is SPP, audio and older HID. Outside those three there is
rarely a reason to choose Classic, and the Classic features EspBle offers follow
the same boundary.

- **SPP**: an RFCOMM byte stream. BLE has no standard serial profile; there you
  define your own GATT service, as Nordic UART Service (NUS) does. If the peer is
  a PC or Android device and expects to see a COM port, that is SPP. Note that
  **iOS apps cannot use SPP** (MFi accessories aside), so for iOS use BLE
- **Audio**: A2DP Sink/Source, AVRCP, HFP Client/Audio Gateway. EspBle handles
  already-encoded payloads and raw SCO. For A2DP SBC codecs and PCM I/O, combine
  it with the released [PCMFlowBluetooth](https://github.com/tanakamasayuki/PCMFlowBluetooth)
  library
- **Older HID**: gamepad, keyboard, mouse. The only route when the peer does not
  accept BLE HID, and a gamepad is the usual reason

## 3. Can HID and SPP run together?

**Yes.** One Classic device running a HID Device and an SPP server at the same
time, with the peer running a HID Host and an SPP client, is hardware-verified
(peer test `classic_hid_report`). HID and SPP are separate profiles over the same
Classic transport, so neither excludes the other.

So a device that wants to be both a HID device and a serial port **can do it with
Classic alone**, which also means there is no need for a BLE-HID-plus-Classic-SPP
dual-host arrangement.

The BLE equivalent is a HID Service and a NUS-like service of your own on the same
GATT server. That works too, and is the straightforward choice when the peer
accepts BLE.

EspBle has three concurrency arrangements:

| Arrangement | Status | Notes |
|---|---|---|
| Several Classic profiles at once (HID + SPP, …) | verified | HFP Client and Audio Gateway exclude each other; A2DP is one role, one session |
| Several BLE services at once (HID + your own) | supported | The bundled host on the original ESP32 allows three connections |
| BLE and Classic at once (dual-host) | experimental | Starting both makes the broker route HCI; see the [dual-host technical guide](GUIDE_DUAL_HOST.md), and `end()` one if it misbehaves |

## 4. Where the shared features differ

### 4.1 HID

Report Descriptors and report packing come from the same module on both
transports, so **the bytes on the air are the same**, and the API names and
signatures match. What differs is everything around them.

| | BLE (HOGP) | Classic (HID over BR/EDR) |
|---|---|---|
| Starting a connection | Find advertising and connect; filter by HID Service `0x1812` | Connect by address; peers are found with inquiry, which has no advertising-style filter |
| Receiving the Report Descriptor | The GATT Report Map characteristic | SDP (`ESP_HIDH_GET_DSCP_EVT`) |
| How many profiles compose | Effectively unlimited (the Report Map is read as a characteristic) | **214 bytes for the descriptor plus the three profile strings** (86 of the SDP record's 300-byte pad go to the standard attributes). A combination that does not fit is refused by `begin()` with `ResourceExhausted` |
| Where the report ID sits | Not in the payload, because each characteristic is separate | First byte of the payload, and the same in the raw value from `onInputReport()` |
| What the Host decodes | keyboard, mouse, consumer, system, gamepad | keyboard and mouse only; the rest arrive raw at `onInputReport()` |
| Battery level | The HID Host can read it | Not read |
| Boot Protocol | Supported (opt-in, off by default) | Not supported |
| Concurrent connections | Three (bundled host on the original ESP32) | One HID Host connection |
| Automatic reconnection | `setAutoReconnect()` / persistent subscriptions / `setAutoRediscover()` | No equivalent; the sketch calls `connect()` |
| Sending LEDs | `setKeyboardLeds(connectionId, ...)` | `setKeyboardLeds(...)`, with no id because there is one connection; the report ID comes from the peer's descriptor |
| Encryption expectations | Commercial keyboards normally require encryption on HID attributes | Pairing and a link key are assumed |

The composition limit is measured on hardware. With the default strings (57 bytes
in total) keyboard + mouse + consumer (144 descriptor bytes) registers, and adding
the gamepad, at 212, does not. A gamepad paired with a keyboard fits in 133 bytes.
BLE has no such limit and composes the same profiles as they are.

Keyboard layouts (`setLayout()` / `setKeyboardLayout()`), NKRO, `pressKey()` /
`tapKey()` / `write()`, the accumulating mouse `wheel()` / `click()` / `press()`,
and the consumer, system and gamepad send calls have the same names and the same
behaviour on both sides.

### 4.2 Security and bonds

| | BLE | Classic |
|---|---|---|
| Method | LE pairing (Just Works / passkey / numeric comparison) | SSP (Just Works / passkey / numeric comparison); legacy PIN is refused |
| Stored key | LE bond key, IRK | Link key |
| Independence | Removing one leaves the other; a BLE bond and a Classic bond are different things | Same |
| How the IO capability applies | Decided by `EspBleConfig::security` | Enabling `EspBleClassicSecurityConfig` makes the service require MITM protection; left disabled the pairing is Just Works and the IO capability has no effect |

### 4.3 Discovery

| | BLE | Classic |
|---|---|---|
| Finding peers | Scanning advertising; filter by service UUID, name or manufacturer data | Inquiry, which yields an address, a name, a Class of Device and an RSSI |
| Being found | Advertising | Being discoverable as well as connectable; a device you can connect to is not necessarily one an inquiry finds |
| Names | Carried in advertising or the scan response | May arrive later than the inquiry result |

### 4.4 Data transfer

| | BLE | Classic |
|---|---|---|
| General-purpose path | GATT (characteristics, notify / indicate, MTU) | SPP's RFCOMM byte stream |
| Boundaries | Per characteristic, bounded by the MTU | A byte stream, binary-safe: `0x00` terminates nothing |
| Serial-compatible API | None (build your own over GATT) | `EspBleClassicSppStream` wraps a session as an Arduino `Stream`; it differs from `Serial` only in that a write becomes one packet and the outgoing queue is finite |

### 4.5 Radio and link settings

| | BLE | Classic |
|---|---|---|
| Transmit power | `EspBle::setTxPower(dBm)` / `txPower()`, one level | `setTxPower(dBm)` and `setTxPower(min, max)` / `txPower()`; BR/EDR power control picks a level per packet from a range, hence the range form. Both are -12..+9 dBm in 3 dB steps |
| Independence | Applies to LE only | Applies to BR/EDR only; a dual-host sketch sets both separately |
| Time to a failed connection | The timeout argument of `connect()` | `setPageTimeout()` (14..40959 ms, default 5120), the time spent paging a peer that answers nothing, applied from the next page |
| Minimum encryption key size | No API | `setMinimumEncryptionKeySize()` (7..16 bytes) |
| A peer's signal strength | The RSSI of a scan result, and readable after connecting | The RSSI of an inquiry result only; the backend's connected RSSI is a delta rather than dBm, so it is not exposed |

## 5. Where the limits come from

Most Classic limits come from the backend (the separately built Classic-only
Bluedroid) and the controller rather than from EspBle's design. The single HID
Host connection, HFP's role exclusion and A2DP's one role per session are all
backend constraints. The original ESP32's controller is BLE 4.2 class, which is
also why LE 2M and Coded PHY, and Extended and Periodic Advertising, are
unavailable on the BLE side.

Classic has shipped since 1.3.0. The project does not promise support or
compatibility for this host; instead each feature's state —
hardware-verified, unverified or unimplemented — is written down in the
[Classic feature inventory](CLASSIC_FEATURE_INVENTORY.ja.md) (Japanese). Running
BLE and Classic together is hardware-verified but stays experimental. The
precompiled Classic host is ABI-bound to ESP-IDF 5.5.5 / xtensa-esp32 GCC
14.2.0; the measured core range is Arduino-ESP32 3.2.0 through 3.3.11, with
HFP audio (SCO) needing 3.3.8 or newer. The history
and the measurements are in the
[Classic implementation plan](PLAN_ESP32_CLASSIC.ja.md) and the
[handover notes](HANDOFF_ESP32_CLASSIC.ja.md) (both Japanese).
