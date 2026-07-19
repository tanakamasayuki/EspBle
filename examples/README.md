# EspBle Examples

> 日本語版: [README.ja.md](README.ja.md)

## What is BLE?

Bluetooth Low Energy (BLE) is a radio standard for exchanging small amounts of data at very low power. Despite the similar name, it is **a different protocol from Bluetooth Classic** (used by headsets and SPP) and the two are not compatible.

- **Bluetooth Classic**: always-connected stream transport for audio (A2DP/HFP) and serial (SPP). Comparatively power-hungry.
- **BLE**: event-oriented, short bursts of communication — sensor values, key input, configuration data. Designed for battery-powered devices.

EspBle is a **BLE-only library** built on the NimBLE backend bundled with Arduino-ESP32. Bluetooth Classic A2DP/HFP/**SPP are not available**. For a serial-like BLE link, use a GATT-based approach such as the NUS-compatible [server](Gatt/NusServer/) and [client](Gatt/NusClient/) examples.

### GAP — discovering and connecting

GAP (Generic Access Profile) is the discovery-and-connection layer.

- **Advertising**: a peripheral broadcasts its name and service UUIDs ("I'm here").
- **Scanning**: a central listens for those broadcasts to find devices.
- **Connecting**: the central picks a scan result and connects. After that, data flows both ways regardless of role.

One ESP32 can act as **central and peripheral at the same time** (e.g. receive input from a keyboard while presenting itself as a keyboard to a PC).

## GATT — exchanging data

GATT (Generic Attribute Profile) is the data model used after connecting. Data is exposed as **services** (functional groups, e.g. Battery Service) containing **characteristics** (individual values, e.g. Battery Level).

- **GATT server**: owns the values; answers reads/writes and pushes value changes to subscribers via **notifications/indications**.
- **GATT client**: uses the values; reads, writes, and subscribes by UUID.

"Peripheral = server, central = client" is the typical combination, but GATT roles are independent of the link role.

### HID — keyboards and mice

HID over GATT (HOGP) carries the same HID model as USB keyboards/mice over BLE. EspBle provides both sides:

- **HID Device**: keyboard, mouse, consumer/system control, and gamepad profiles can be composed in one HID Service.
- **HID Host**: one `hidHost()` receives all supported report types; keyboard input is also layout-converted to Unicode/ASCII (19 layouts).

### Security — pairing, bonding, encryption

BLE security is established per connection.

- **Pairing**: the key exchange that encrypts the link — **Just Works** (no confirmation) or **MITM-authenticated** passkey entry (6-digit code).
- **Bonding**: stores the exchanged keys so reconnections encrypt automatically.
- **Attribute protection**: each characteristic can require an encrypted or authenticated link; insufficient security is rejected at the ATT layer (HID keyboards require encryption by specification).

In EspBle, enable security via `EspBleConfig::security` and declare per-characteristic requirements with `encryptedRead/Write` / `authenticatedRead/Write`. [Security/JustWorksServer](Security/JustWorksServer/) and [Security/StaticPasskeyServer](Security/StaticPasskeyServer/) are the minimal setups.

## Building

Every example ships with a `sketch.yaml` profile pinned to the verified Arduino-ESP32 version, so it can be built without IDE board setup:

```sh
arduino-cli compile --profile esp32s3 examples/<path>
```

## Index

| Example | Role | Description |
|---|---|---|
| [CompileSmoke](CompileSmoke/) | — | Minimal build check; prints the library version |
| [Gap/Advertise](Gap/Advertise/) | Peripheral | Connectable legacy advertising with name + service UUID |
| [Gap/Scan](Gap/Scan/) | Central | Continuous active scan printing address / RSSI / name |
| [Gap/Connect](Gap/Connect/) | Central | Scan for a service UUID and connect; async connect/disconnect/failure events |
| [Gap/Mtu](Gap/Mtu/) | Central | Preferred-MTU exchange and notification payload limits |
| [Gatt/Server](Gatt/Server/) | Peripheral | Custom service with a readable/writable characteristic |
| [Gatt/Client](Gatt/Client/) | Central | Known-UUID discovery → read → write chain against Gatt/Server |
| [Gatt/NotifyServer](Gatt/NotifyServer/) | Peripheral | Subscription-gated periodic notifications |
| [Gatt/SubscribeClient](Gatt/SubscribeClient/) | Central | Subscribe to NotifyServer and print notifications |
| [Gatt/IndicateServer](Gatt/IndicateServer/) | Peripheral | Acknowledged indications with `onSent()` delivery confirmation |
| [Gatt/IndicateClient](Gatt/IndicateClient/) | Central | Subscribe to IndicateServer's indications |
| [Gatt/BatteryServer](Gatt/BatteryServer/) | Peripheral | Standard Battery Level reads and notifications |
| [Gatt/BatteryClient](Gatt/BatteryClient/) | Central | Read and subscribe to Battery Level |
| [Gatt/DeviceInfoServer](Gatt/DeviceInfoServer/) | Peripheral | Standard Device Information strings and PnP ID |
| [Gatt/DeviceInfoClient](Gatt/DeviceInfoClient/) | Central | Sequential Device Information reads and PnP ID decoding |
| [Gatt/CurrentTimeServer](Gatt/CurrentTimeServer/) | Peripheral | Standard 10-byte Current Time reads and notifications |
| [Gatt/CurrentTimeClient](Gatt/CurrentTimeClient/) | Central | Current Time decoding and notification subscription |
| [Gatt/HeartRateServer](Gatt/HeartRateServer/) | Peripheral | Standard Heart Rate Measurement and Body Sensor Location |
| [Gatt/HeartRateClient](Gatt/HeartRateClient/) | Central | Flags-driven Heart Rate Measurement decoding and subscription |
| [Gatt/EnvironmentalServer](Gatt/EnvironmentalServer/) | Peripheral | Standard Temperature, Humidity, and Pressure values |
| [Gatt/EnvironmentalClient](Gatt/EnvironmentalClient/) | Central | Scaled sensor reads and Temperature notification subscription |
| [Gatt/NusServer](Gatt/NusServer/) | Peripheral | NUS-compatible RX writes and TX notification echo |
| [Gatt/NusClient](Gatt/NusClient/) | Central | NUS-compatible TX subscription and RX Write Without Response |
| [Security/JustWorksServer](Security/JustWorksServer/) | Peripheral | Encrypted characteristic with Just Works pairing + bonding |
| [Security/StaticPasskeyServer](Security/StaticPasskeyServer/) | Peripheral | MITM-authenticated characteristic with a static passkey (display side) |
| [Security/StaticPasskeyClient](Security/StaticPasskeyClient/) | Central | Passkey input side: `requestSecurity()` and authenticated reads |
| [Hid/KeyboardDevice](Hid/KeyboardDevice/) | HID Device | BLE keyboard typing via Serial commands, LED report reception |
| [Hid/KeyboardHost](Hid/KeyboardHost/) | HID Host | Connect to composite BLE HID, print every supported report type, write keyboard LEDs |
| [Hid/Mouse](Hid/Mouse/) | HID Device | Five-button relative mouse |
| [Hid/ConsumerControl](Hid/ConsumerControl/) | HID Device | Volume and play/pause media keys |
| [Hid/CompositeKeyboardMouse](Hid/CompositeKeyboardMouse/) | HID Device | One composite HID Service with keyboard and mouse reports |
| [Hid/VendorDevice](Hid/VendorDevice/) | HID Device | Report ID 6 Vendor Input / Output / Feature |
| [Hid/VendorHost](Hid/VendorHost/) | HID Host | Vendor Input reception and Output / Feature writes |
| [Info/ScanDump](Info/ScanDump/) | Diagnostics | Dump every advertisement field (UUIDs, manufacturer data, …) |
| [Info/ConnectionInspector](Info/ConnectionInspector/) | Diagnostics | Interactively connect and dump MTU, security state, bonds, counters |

Suggested pairings on two boards:

- Gap/Advertise ↔ Gap/Scan
- Gatt/Server ↔ Gatt/Client
- Gatt/NotifyServer ↔ Gatt/SubscribeClient (and Gap/Mtu)
- Gatt/IndicateServer ↔ Gatt/IndicateClient
- Gatt/BatteryServer ↔ Gatt/BatteryClient
- Gatt/DeviceInfoServer ↔ Gatt/DeviceInfoClient
- Gatt/CurrentTimeServer ↔ Gatt/CurrentTimeClient
- Gatt/HeartRateServer ↔ Gatt/HeartRateClient
- Gatt/EnvironmentalServer ↔ Gatt/EnvironmentalClient
- Gatt/NusServer ↔ Gatt/NusClient
- Security/StaticPasskeyServer ↔ Security/StaticPasskeyClient
- Hid/KeyboardDevice / Hid/CompositeKeyboardMouse ↔ Hid/KeyboardHost
- Hid/VendorDevice ↔ Hid/VendorHost
- Info/ScanDump and Info/ConnectionInspector can observe anything — the other examples, smartphones, or commercial BLE devices
