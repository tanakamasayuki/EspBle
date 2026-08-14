# Feature support matrix

> 日本語版: [FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)

An overview of what EspBle supports: the BLE equivalents of what EspUsbHost / EspUsbDevice cover, plus the features commonly used with BLE. For the settled priorities, [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) and [DECISIONS.ja.md](DECISIONS.ja.md) are authoritative; this table is a bird's-eye draft.

## Legend

| Symbol | Meaning |
|---|---|
| ✅ | Supported (implemented and verified by peer / unit tests) |
| 🔧 | Needs a feature addition (a profile or API has to be added to the library itself) |
| 📝 | Achievable in an example alone (writable by combining existing public APIs; no library change) |
| ⚠️ | Partially working (receivable but not distinguishable, and so on; the behaviour is stated in the notes) |
| ❌ | Not supportable (outside the scope of BLE/NimBLE, or already decided as out of scope) |

## BLE basics

| Feature | Status | Notes |
|---|---|---|
| Legacy advertising | ✅ | Name / service UUID / manufacturer data / service data / appearance / Tx power, connectable and non-connectable, advertising interval control |
| Scan response payload | ✅ | `EspBleAdvertising::scanResponse()` returns the same builder as the advertising payload, giving a second 31 bytes. When it is unset and the scan response is enabled, the device name is placed there automatically. The passive/active difference is verified by the `scan_response` peer test |
| Scanning (active/passive, value-type scan results) | ✅ | `Gap/Scan`, `Info/ScanDump`. A scan result holds address / addressType / rssi / connectable / scannable / name / serviceUuids / serviceData (up to 4 blocks plus UUID lookup) / manufacturerData / appearance / txPowerLevel |
| Central connect / peripheral accept / disconnect | ✅ | Connect from a scan result or an address directly, connection ID management, several simultaneous connections (the maximum comes from the bundled NimBLE controller — 3 on the ESP32-S3), auto-reconnect (`setAutoReconnect`, off by default) |
| GATT server (custom services, characteristics, descriptors) | ✅ | Arbitrary UUIDs, permissions, binary-safe values, descriptor write events. Registration is the handle chain `addService()` → `addCharacteristic(service, …)` → `addDescriptor(characteristic, …)`, and later value operations, sends and events identify their target by handle |
| Several services with the same UUID | ✅ | **Registrable on the peripheral side**: the attribute table is built directly with `ble_gatts_add_svcs()`, and the handle returned by `addService()` identifies the target. **Distinguishable on the central side too**: discovery uses `ble_gattc_disc_all_svcs()`, and reads, writes and CCCD writes are issued against attribute handles, so the second and later services are reachable. Notifications are matched from `BLE_GAP_EVENT_NOTIFY_RX` by value handle. Read, subscribe and notify are all verified by the `duplicate_uuid` peer test |
| Same-UUID characteristics within one service | ✅ | **Registrable on the peripheral side too**: the attribute table is built directly with `ble_gatts_add_svcs()`, and the target is identified by the definition pointer the access callback passes back. **Distinguishable on the central side too** (discovery enumerates per attribute handle, and operations name a handle). The `duplicate_uuid` peer test exposes two same-UUID characteristics in one service plus two same-UUID services, and verifies read, subscribe and notify against all three. |
| GATT client (enumeration / known-UUID discovery, read, write) | ✅ | Per-connection discovery snapshot, descriptor operations (by UUID and **by attribute handle** — a descriptor of a characteristic whose UUID repeats, such as a HID Report Reference 0x2908, is only expressible by handle), write without response, per-operation timeout, automatic operation queueing (eight queued beside the one in flight). Generic client operations go straight to the NimBLE host API rather than the bundled wrapper's remote objects (to handle duplicate UUIDs and to avoid the discovery heap leak) |
| Notify / indicate (subscribe, unsubscribe, CCCD) | ✅ | `Gatt/Basics/NotifyServer`, `Gatt/Basics/IndicateServer`. Persistent subscriptions (`EspBleConfig::persistentSubscriptions`, on by default) re-subscribe automatically on reconnect. The restore keys on the peer address and the UUID, so it is **limited to characteristics with a unique UUID** (with duplicates there is no way to say which was subscribed, so nothing is recorded) |
| MTU exchange / payload limit verification | ✅ | The default `preferredMtu` is **247** (a 244-byte notify payload). The central starts the exchange with `ble_gattc_exchange_mtu()` immediately after the connection comes up. The result is tracked for both roles through `BLE_GAP_EVENT_MTU` and delivered to `onMtuChanged`, so **`connection.mtu` at `onConnected` is 23 on both roles** (the default) |
| Reading values larger than the MTU | ✅ | Characteristic and descriptor reads are issued with `ble_gattc_read_long()`, so a value that does not fit one MTU is read to the end (a few-hundred-byte value such as a HID report map is not silently truncated). A short value completes in the first response, so there is no extra round trip |
| GATT server read hook | ✅ | `EspBleGattServer::onRead()`. Called the moment a peer reads a characteristic; whatever `setValue()` receives there is what goes out (for producing a sensor value on demand). **It runs on the BLE stack task**, so heavy work there stalls the stack and the peer's read times out |
| Composing several services | ✅ | HID + DIS + Battery composition implemented |
| Just Works pairing / bonding | ✅ | LE Secure Connections |
| Static passkey / MITM authentication / encrypted and authenticated permissions | ✅ | The `Security/*` examples |
| Runtime passkey entry | ✅ | With KeyboardOnly + MITM and no static passkey, `providePasskey()` supplies the value during pairing. The display side is DisplayOnly with no static passkey, generating a passkey dynamically and reporting it through `onPasskeyDisplayed`. The wait is abandoned after 30 seconds. Peer-verified, with the `Security/RuntimePasskey{Server,Client}` examples |
| Numeric Comparison | ✅ | LE Secure Connections, DisplayYesNo + MITM on both sides. `onNumericComparison` presents the comparison value and `confirmNumericComparison()` answers. The wait is abandoned after 30 seconds. Peer-verified, with the `Security/NumericComparison{Server,Client}` examples |
| Privacy (own address type: public / random static / RPA) | ✅ | `EspBleConfig::ownAddressType`. `RandomStatic` is a fixed random static address; `ResolvablePrivate` is a controller-rotated RPA (`CONFIG_BT_NIMBLE_RPA_TIMEOUT` = 900 s, resolved by the peer with the IRK when bonding is used). Random static advertising is verified by the `address_privacy` peer test |

## HID profiles (the area with the closest USB parallels)

| Feature | USB side | Status | Notes |
|---|---|---|---|
| HID keyboard device | ✅ | ✅ | 6KRO / NKRO, LED output, 19-layout integration |
| HID keyboard host | ✅ | ✅ | 6KRO / NKRO parser, usage snapshot, Unicode conversion |
| HID mouse (device / host) | ✅ | ✅ | Relative movement, wheel, 5 buttons |
| HID consumer control (media keys) | ✅ | ✅ | 16-bit usages |
| HID gamepad | ✅ | ✅ | 6 axes, hat, 32 buttons, per-field host dispatch |
| HID system control (power and so on) | ✅ | ✅ | 8-bit usages |
| Composite HID (keyboard + mouse, …) | ✅ | ✅ | Fixed report IDs, several input reports |
| Vendor HID report | ✅ | ✅ | Fixed ID 6, input / output / feature, device and host peer-verified |
| Custom HID with an arbitrary report descriptor | ✅ | ✅ | `ble.hidCustom()` takes a raw report map plus arbitrary report declarations. Composed into the same HID service as the built-in profiles. Input and output are peer-verified on hardware (same-UUID reports are addressed by handle from the client). Up to 4 reports per device |
| NKRO | ✅ | ✅ | The EspUsbDevice-compatible 29-byte bitmap, device and host peer-verified |
| Boot Protocol switching (keyboard) | ✅ | ✅ | Opt-in (`EspBleHidKeyboardConfig::bootProtocol`, off by default). Protocol Mode 0x2A4E plus Boot Keyboard Input/Output 0x2A22/0x2A32. Boot mode switches automatically to the 8-byte boot report, with `onProtocolMode()`. The mouse boot report 0x2A33 is not supported |

## Other profiles and services

| Feature | USB side | Status | Notes |
|---|---|---|---|
| Battery Service | — | ✅ | Built into HID plus standalone server/client examples, peer-verified |
| Device Information Service (PnP ID and so on) | Info | ✅ | Built into HID plus standalone server/client examples. The PnP ID wire format is peer-verified |
| Serial equivalent (CDC ACM → Nordic UART Service) | ✅ | 📝 | `Gatt/Basics/NusServer` / `NusClient`. Packet framing is the application's responsibility |
| MIDI (USB MIDI → BLE MIDI) | ✅ | ✅ | The BLE MIDI service. A codec for timestamps, running status and multi-packet SysEx (unit-tested) plus the `EspBleMidiDevice` / `EspBleMidiHost` profile helpers. The device wire format and host decoding are peer-verified |
| Custom / vendor GATT services (the vendor-bulk equivalent) | ✅ | ✅ | Any service can be built with the existing GATT server API (`Gatt/Basics/Server`) |
| Heart Rate Service | — | ✅ | Body sensor location plus a variable-length measurement, verified with server/client examples and peer tests |
| Environmental Sensing Service | — | ✅ | Temperature / humidity / pressure, with server/client examples, peer-verified |
| Health Thermometer Service | — | ✅ | Temperature type read plus IEEE-11073 32-bit FLOAT temperature measurement indications. The medical float codec (unit-tested) plus server/client examples, peer-verified |
| Blood Pressure Service | — | ✅ | Feature read plus IEEE-11073 16-bit SFLOAT (systolic/diastolic/mean) measurement indications. Server/client examples, peer-verified |
| Weight Scale Service | — | ✅ | Feature read plus a uint16 weight measurement with 0.005 kg resolution, indicated. Server/client examples, peer-verified |
| Body Composition Service | — | ✅ | Feature read plus uint16 flags, the mandatory body fat percentage (0.1 %/LSB) and an optional weight, indicated. Server/client examples, peer-verified |
| Cycling Speed and Cadence Service | — | ✅ | Feature / sensor location reads plus a multi-field CSC measurement (wheel and crank revolutions), notified. Server/client examples, peer-verified |
| Running Speed and Cadence Service | — | ✅ | Feature / sensor location reads plus a mixed-width RSC measurement (speed / cadence / stride / distance), notified. Server/client examples, peer-verified |
| Cycling Power Service | — | ✅ | Feature / sensor location reads plus a CP measurement with 16-bit flags and signed 16-bit power, notified. Server/client examples, peer-verified |
| Fitness Machine Service (FTMS) | — | ✅ | Fitness Machine Feature (0x2ACC) read plus flags-driven Indoor Bike Data (0x2AD2: speed 0.01 km/h, cadence 0.5/min, signed power in W) notified, plus the Fitness Machine Control Point (0x2AD9, write + indicate) and Fitness Machine Status (0x2ADA). Request Control / Set Target Power produce a response indication, and Set Target Power also notifies a "Target Power Changed" status. Server/client examples, peer-verified for both data and control |
| Pulse Oximeter Service (PLX) | — | ✅ | Features read plus IEEE-11073 16-bit SFLOAT (SpO2 / pulse rate) spot-check measurement indications. Server/client examples, peer-verified |
| Glucose Service (RACP) | — | ✅ | The Record Access Control Point procedure (write → measurement notify → RACP response indicate). Sequence number, base time and SFLOAT concentration verified with server/client examples and peer tests |
| Location and Navigation Service | — | ✅ | LN feature read plus flags-driven location-and-speed notifications (instantaneous speed, sint32 latitude and longitude). Server/client examples, peer-verified |
| User Data Service | — | ✅ | Read/write of age and first name; writes arrive at `onWritten` and the database change increment is notified. The write → onWritten → notify path is verified with server/client examples and peer tests |
| Alert Notification Service | — | ✅ | Supported New Alert Category bitmask read, and an Alert Notification Control Point write producing a New Alert notification with category, count and text. The control-point → notify path is verified with server/client examples and peer tests |
| Immediate Alert Service (Find Me) | — | ✅ | Alert level write without response received at `onWritten` (the Find Me target role). The write-without-response path is verified with server/client examples and peer tests |
| Phone Alert Status Service | — | ✅ | Read/notify of alert status and ringer setting, with the Ringer Control Point (write without response) switching silent mode and notifying the ringer setting. The control-point → state-change-notify path is verified with server/client examples and peer tests |
| Proximity (Link Loss + Tx Power) | — | ✅ | One server hosting both the Link Loss Service (alert level read/write) and the Tx Power Service (signed int8 Tx power level read). Verified with server/client examples and peer tests |
| Reference Time Update Service | — | ✅ | The Time Update Control Point (write without response) transitions a read-only Time Update State (current state plus result). The control-point → state-transition path is verified with server/client examples and peer tests |
| Bond Management Service | — | ✅ | Bond Management Feature (uint24 bit field) read plus op-code writes to the Bond Management Control Point. The server example deletes the relevant bond after disconnecting. The GATT choreography is verified with server/client examples and peer tests |
| Continuous Glucose Monitoring Service | — | ✅ | E2E-CRC-protected CGM feature read plus CGM measurement notifications carrying an SFLOAT glucose value and a time offset. The E2E-CRC is shared through `EspBleCgmCrc.h` (CRC-16/MCRF4XX, unit-tested), verified with server/client examples and peer tests |
| Other standard sensor services | — | 📝🔧 | 📝 if you register the standard UUIDs on the GATT server yourself; 🔧 for a profile helper. IEEE-11073 medical floats (including SFLOAT) are shared through `EspBleMedicalFloat.h` |
| Current Time Service | — | ✅ | Standalone server/client examples. The 10-byte wire format and notifications are peer-verified |
| Other standard services | — | 📝 | Buildable in an example as a GATT server using the standard UUIDs |
| OTA / DFU | — | 📝❌ | 📝 to build your own over a custom GATT service; providing a unified OTA/DFU scheme is ❌ out of scope |
| Mass storage (the USB MSC equivalent) | ✅ | ❌ | No practical equivalent in BLE (bandwidth, and no such profile) |
| USB audio (the UAC equivalent) | ✅ | ❌ | LE Audio is a separate stack and out of scope |
| Networking (the CDC-NCM equivalent) | ✅ | ❌ | BLE's IPSP/6LoWPAN is not practical |
| Hubs / topology | ✅ | ❌ | BLE has no hub concept (several connections serve as the alternative) |
| Several devices connected at once | ✅ | ✅ | Separated by per-connection cache, subscriptions and GATT routing. The maximum is set by the bundled NimBLE controller (3 on the ESP32-S3). Verified by the three-board manual test `multi_connection` |

## Advanced GAP / link features

| Feature | Status | Notes |
|---|---|---|
| Beacon (non-connectable broadcaster) | ✅ | `setConnectable(false)` plus `setScanResponseEnabled(false)` plus `setInterval()`. The payload is built with `setManufacturerData()` and friends. Peer-verified on hardware |
| iBeacon (the Apple beacon layout) | ✅ | The backend-independent codec `EspBleIBeacon.h` (`espBleEncodeIBeacon` / `espBleDecodeIBeacon`). Company ID 0x004C plus UUID plus major/minor plus measured power. Broadcast and decode verified by unit tests and the `ibeacon` peer test |
| Advertising service data (AD 0x16) | ✅ | Up to 4 blocks sent with `EspBleAdvertising::addServiceData(uuid, data, length)`, received through `EspBleScanResult::serviceData[]` / `serviceDataCount` / `serviceDataFor(uuid, data)`. Multiple blocks and UUID lookup verified by the `service_data` peer test |
| Filter Accept List (restricting connections on the peripheral side) | ✅ | `EspBle::addToAcceptList()` plus `EspBleAdvertising::setFilterPolicy()` (Any / ScanRequest / Connection / Both). The controller rejects, so nothing reaches the application. The bundled wrapper's white-list API does not link, so `ble_gap_wl_set()` is used directly. Verified by the `accept_list` peer test |
| Directed advertising (sending) | ✅ | `EspBleAdvertising::setDirectedTarget(address, addressType, highDuty)` / `clearDirectedTarget()`, calling `ble_gap_adv_start()` directly. The specification carries no AD data, so no payload is sent and only the named peer may connect. High duty cycle transmits every 3.75 ms for up to 1.28 s. If the peer uses an RPA, resolution goes through the bond, so bonding must happen first. Verified by the `directed_advertising` peer test |
| Directed advertising (receiving) | ⚠️ | An ADV_DIRECT_IND addressed to this device arrives as a scan result carrying address / addressType / rssi / connectable=true / scannable=false (the rest is empty, since the specification carries no AD data). It can be connected to as usual. However, **the advertisement type is not exposed, so it cannot be identified**; it has to be inferred from "connectable, non-scannable, empty payload" |
| Filter Accept List on the scanning side | ✅ | `EspBleScanConfig::acceptListOnly`. It is passed as the filter policy of `ble_gap_disc()`, so advertisements from peers not on the list are dropped by the controller and never reach the application. The `accept_list` peer test verifies that nothing is reported with an empty list and that the peer is reported once its address is added |
| Changing transmit power | ✅ | `EspBle::setTxPower(dBm)` / `txPower()`, rounded to the discrete values the radio supports (−12..+9 dBm in 3 dB steps). The `local_identity` peer test verifies that the configured value appears on air as the advertised Tx power level |
| Reading this device's address | ✅ | `EspBle::localAddress()` / `localAddressType()`: the current value, which changes on every rotation when an RPA is in use. The `local_identity` peer test verifies it matches what the peer observes while scanning |
| Specifying parameters / PHY at connect time | 🔧 | `connect()` cannot specify a connection interval or PHY. Change them after the connection is established with `updateConnectionParameters()` / `updatePhy()` |
| Specifying a disconnect reason (sender side) | ✅ | `disconnect(id, reason)`. `disconnectReason` is normalised to the HCI code before it is exposed (the backend reports it with a 0x200 offset), so the value passed appears unchanged at the peer. Verified by the `local_identity` peer test |
| Choosing the advertising channel map | ✅ | `EspBleAdvertising::setChannelMap(mask)` (a bit mask of `EspBleAdvertisingChannel37/38/39`; 0 restores all three). It avoids channels that overlap Wi-Fi, at the cost of taking longer to be found. Connecting with channel 39 alone is verified by the `directed_advertising` peer test |
| Extended advertising / several advertising sets | ❌ | The bundled NimBLE is built with `CONFIG_BT_NIMBLE_EXT_ADV` disabled and an Arduino library cannot enable it |
| Periodic advertising | ❌ | Depends on extended advertising (`CONFIG_BT_NIMBLE_EXT_ADV`), so unsupportable for the same reason |
| 2M PHY / coded PHY (long range) | ✅ | Supported through a PHY update after connecting (see "PHY update" below). 2M is peer-verified on hardware; coded (long range) depends on radio support. **Unavailable on the original ESP32, which has a BLE 4.2 controller.** |
| Connection parameter update | ✅ | `updateConnectionParameters()` requests it and `onConnectionParametersUpdated()` delivers the result. `EspBleConnection` exposes interval / latency / timeout. Both roles and both paths are peer-verified |
| PHY update (2M / coded) | ✅ | `updatePhy()` requests it and `onPhyUpdated()` delivers the result. `EspBleConnection` exposes the tx/rx PHY. Updating to the 2M PHY is peer-verified (coded depends on radio support). **Unavailable on the original ESP32, which has a BLE 4.2 controller.** |
| Reading the disconnect reason | ✅ | `EspBleConnection::disconnectReason` (the backend/HCI reason code, in onDisconnected). Both the server and client paths are peer-verified |
| GATT Service Changed | ✅ | The server sends the 0x1801/0x2A05 indication with `notifyServicesChanged()`; a client can subscribe, receive it and decode the range. Peer-verified (rediscovery on receipt is the application's decision) |

## Bluetooth Classic (BR/EDR) — original ESP32 only

ESP32-S3/C3/C6/H2 and similar targets have no Bluetooth Classic radio. On the original ESP32, using `EspBleClassic`
automatically selects the custom-built Classic-only Bluedroid host. Classic is in scope for the next release with no support or compatibility guarantee; what each ⚠️ row states is what has been verified on hardware, and interoperability with external devices is unverified. Dual-host support remains experimental.

| Feature | Status | Notes |
|---|---|---|
| Bluetooth Classic (BR/EDR) in general | ⚠️ | Selected automatically when `EspBleClassic` is used on the original ESP32; unsupported on other SoCs |
| A2DP (audio streaming) | ⚠️ | Sink/Source SBC negotiation and encoded-payload transport; codec/PCM/device I/O stays in another library |
| HFP (hands-free) | ⚠️ | Client/Audio Gateway SLC, outgoing/incoming/answer/end, selectable CVSD/mSBC raw SCO, role exclusion, and the Client's operator name, subscriber number, memory dial, NREC and Apple battery reporting are implemented and hardware-verified; call waiting and three-way calling (CHLD, BTRH) are not implemented, and external-device interoperability remains |
| AVRCP (media control) | ⚠️ | CT/TG passthrough, metadata/play-status requests, and absolute volume; external-target metadata interoperability remains |
| SPP (Serial Port Profile) | ⚠️ | Classic-only Server/Client transport verified on hardware. `EspBleClassicSppStream` presents a session as an Arduino `Stream` (a write becomes one packet, and the outgoing queue is finite). Use NUS or similar on BLE |
| Classic HID (BT HID) | ⚠️ | Classic-only generic Device/Host and the control channel (Get_Report / Set_Report / protocol mode / idle rate / virtual cable unplug) verified on hardware. How many profiles can be composed is capped by the SDP record at 214 bytes of descriptor plus strings, and the host side decodes keyboard and mouse only. Use HOGP on BLE |
| Classic device discovery / pairing / bond | ⚠️ | Inquiry (name, Class of Device, RSSI), SDP queries, IO capability selection, application answers for numeric comparison and passkey, and bond listing and removal verified on hardware; legacy PIN pairing is refused |
| Classic radio and link settings | ⚠️ | Transmit power (range or single value), page timeout and minimum encryption key size verified on hardware; connected RSSI, QoS, AFH and EIR composition stay unexposed |
| Simultaneous Classic / BLE (dual host) | ⚠️ | Experimental, with no build flag — which hosts a sketch starts decides it; HID/security/lifecycle, bidirectional HFP mSBC SCO, A2DP encoded-media streaming, AVRCP control, and GATT reads during and after each audio link are hardware-verified |

## Notes

- "📝 achievable in an example alone" means it can be written with the current API, registering services and characteristics with arbitrary UUIDs through `ble.gattServer()` and calling `notify` / `indicate`. Formally supporting it as a standard profile (a profile helper, dedicated events, wire-format verification) makes it 🔧.
- The BLE versions of the USB-derived features (custom HID with an arbitrary descriptor, BLE MIDI, several simultaneous connections) are all supported, with examples and peer/unit tests. For future unimplemented candidates, see the "priority candidates" section of [DECISIONS.ja.md](DECISIONS.ja.md).
- Among the ❌ entries, MSC, audio, networking, Mesh and LE Audio are either outside BLE's technical scope or belong to a separate stack or library, and are out of scope.
