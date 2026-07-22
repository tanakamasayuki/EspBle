# EspBle Examples

> 日本語版: [README.ja.md](README.ja.md)

## What is BLE?

Bluetooth Low Energy (BLE) is a radio standard for exchanging small amounts of data at very low power. Despite the similar name, it is **a different protocol from Bluetooth Classic** (used by headsets and SPP) and the two are not compatible.

- **Bluetooth Classic**: always-connected stream transport for audio (A2DP/HFP) and serial (SPP). Comparatively power-hungry.
- **BLE**: event-oriented, short bursts of communication — sensor values, key input, configuration data. Designed for battery-powered devices.

EspBle is a **BLE-only library** built on the NimBLE backend bundled with Arduino-ESP32. Bluetooth Classic A2DP/HFP/**SPP are not available**. For a serial-like BLE link, use a GATT-based approach such as the NUS-compatible [server](Gatt/Basics/NusServer/) and [client](Gatt/Basics/NusClient/) examples.

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

### MIDI — BLE MIDI instruments

BLE MIDI carries MIDI messages over a single GATT characteristic with a 13-bit millisecond timestamp per message. EspBle provides both sides on top of the packet codec in `EspBleMidi.h`:

- **MIDI Device** (`EspBleMidiDevice`): advertise a BLE MIDI peripheral and send Note On/Off, Control Change, and other messages.
- **MIDI Host** (`EspBleMidiHost`): connect to a BLE MIDI peripheral, subscribe, and receive decoded messages (running status and System Real-Time handled).

The API mirrors the [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) / [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) MIDI classes so code ports across USB and BLE.

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

Examples are grouped by area. Each standard-service directory holds a matching
`…Server` (peripheral) and `…Client` (central); pair the two on two boards.

### Getting started

| Example | Role | Description |
|---|---|---|
| [CompileSmoke](CompileSmoke/) | — | Minimal build check; prints the library version |

### GAP — advertise, scan, connect

| Example | Role | Description |
|---|---|---|
| [Gap/Advertise](Gap/Advertise/) | Peripheral | Connectable legacy advertising with name + service UUID |
| [Gap/Scan](Gap/Scan/) | Central | Continuous active scan printing address / RSSI / name |
| [Gap/Connect](Gap/Connect/) | Central | Scan for a service UUID and connect; async connect/disconnect/failure events |
| [Gap/Mtu](Gap/Mtu/) | Central | Preferred-MTU exchange and notification payload limits |
| [Gap/Beacon](Gap/Beacon/) | Broadcaster | Non-connectable, non-scannable beacon with manufacturer data and interval control |
| [Gap/IBeacon](Gap/IBeacon/) | Broadcaster | Broadcast an Apple iBeacon (UUID / major / minor / measured power) |
| [Gap/PrivateAddress](Gap/PrivateAddress/) | Peripheral | Advertise with a random static / resolvable private address |

### GATT — Basics (generic mechanics + serial)

| Example | Role | Description |
|---|---|---|
| [Gatt/Basics/Server](Gatt/Basics/Server/) | Peripheral | Custom service with a readable/writable characteristic |
| [Gatt/Basics/Client](Gatt/Basics/Client/) | Central | Known-UUID discovery → read → write chain against the Server |
| [Gatt/Basics/NotifyServer](Gatt/Basics/NotifyServer/) | Peripheral | Subscription-gated periodic notifications |
| [Gatt/Basics/SubscribeClient](Gatt/Basics/SubscribeClient/) | Central | Subscribe to NotifyServer and print notifications |
| [Gatt/Basics/AutoReconnectClient](Gatt/Basics/AutoReconnectClient/) | Central | Auto-reconnect + persistent subscription: notifications resume after a drop |
| [Gatt/Basics/IndicateServer](Gatt/Basics/IndicateServer/) | Peripheral | Acknowledged indications with `onSent()` delivery confirmation |
| [Gatt/Basics/IndicateClient](Gatt/Basics/IndicateClient/) | Central | Subscribe to IndicateServer's indications |
| [Gatt/Basics/NusServer](Gatt/Basics/NusServer/) | Peripheral | NUS-compatible RX writes and TX notification echo |
| [Gatt/Basics/NusClient](Gatt/Basics/NusClient/) | Central | NUS-compatible TX subscription and RX Write Without Response |

### GATT — Device, time & management

| Example | Role | Description |
|---|---|---|
| [Gatt/Device/BatteryServer](Gatt/Device/BatteryServer/) | Peripheral | Standard Battery Level reads and notifications |
| [Gatt/Device/BatteryClient](Gatt/Device/BatteryClient/) | Central | Read and subscribe to Battery Level |
| [Gatt/Device/DeviceInfoServer](Gatt/Device/DeviceInfoServer/) | Peripheral | Standard Device Information strings and PnP ID |
| [Gatt/Device/DeviceInfoClient](Gatt/Device/DeviceInfoClient/) | Central | Sequential Device Information reads and PnP ID decoding |
| [Gatt/Device/UserDataServer](Gatt/Device/UserDataServer/) | Peripheral | Read/write Age and First Name, notify Database Change Increment on writes |
| [Gatt/Device/UserDataClient](Gatt/Device/UserDataClient/) | Central | Write Age/First Name and observe Database Change Increment notifications |
| [Gatt/Device/BondManagementServer](Gatt/Device/BondManagementServer/) | Peripheral | Bond Management Feature read, Control Point delete-bond op codes |
| [Gatt/Device/BondManagementClient](Gatt/Device/BondManagementClient/) | Central | Read the Feature bit field and write a delete-bond op code |
| [Gatt/Time/CurrentTimeServer](Gatt/Time/CurrentTimeServer/) | Peripheral | Standard 10-byte Current Time reads and notifications |
| [Gatt/Time/CurrentTimeClient](Gatt/Time/CurrentTimeClient/) | Central | Current Time decoding and notification subscription |
| [Gatt/Time/ReferenceTimeUpdateServer](Gatt/Time/ReferenceTimeUpdateServer/) | Peripheral | Time Update Control Point drives a readable Time Update State |
| [Gatt/Time/ReferenceTimeUpdateClient](Gatt/Time/ReferenceTimeUpdateClient/) | Central | Request/cancel a reference update and read the Time Update State |

### GATT — Sensors

| Example | Role | Description |
|---|---|---|
| [Gatt/Sensors/EnvironmentalServer](Gatt/Sensors/EnvironmentalServer/) | Peripheral | Standard Temperature, Humidity, and Pressure values |
| [Gatt/Sensors/EnvironmentalClient](Gatt/Sensors/EnvironmentalClient/) | Central | Scaled sensor reads and Temperature notification subscription |

### GATT — Health & body

| Example | Role | Description |
|---|---|---|
| [Gatt/Health/HeartRateServer](Gatt/Health/HeartRateServer/) | Peripheral | Standard Heart Rate Measurement and Body Sensor Location |
| [Gatt/Health/HeartRateClient](Gatt/Health/HeartRateClient/) | Central | Flags-driven Heart Rate Measurement decoding and subscription |
| [Gatt/Health/HealthThermometerServer](Gatt/Health/HealthThermometerServer/) | Peripheral | IEEE-11073 FLOAT Temperature Measurement indications and Temperature Type |
| [Gatt/Health/HealthThermometerClient](Gatt/Health/HealthThermometerClient/) | Central | Temperature Type read and FLOAT measurement indication decoding |
| [Gatt/Health/BloodPressureServer](Gatt/Health/BloodPressureServer/) | Peripheral | IEEE-11073 SFLOAT systolic/diastolic/mean Measurement indications and Feature |
| [Gatt/Health/BloodPressureClient](Gatt/Health/BloodPressureClient/) | Central | Feature read and SFLOAT measurement indication decoding |
| [Gatt/Health/WeightScaleServer](Gatt/Health/WeightScaleServer/) | Peripheral | uint16 Weight Measurement indications (0.005 kg resolution) and Feature |
| [Gatt/Health/WeightScaleClient](Gatt/Health/WeightScaleClient/) | Central | Feature read and Weight Measurement indication decoding |
| [Gatt/Health/BodyCompositionServer](Gatt/Health/BodyCompositionServer/) | Peripheral | Body Fat Percentage + optional Weight Measurement indications and Feature |
| [Gatt/Health/BodyCompositionClient](Gatt/Health/BodyCompositionClient/) | Central | Feature read and Body Fat Percentage / Weight measurement decoding |
| [Gatt/Health/PulseOximeterServer](Gatt/Health/PulseOximeterServer/) | Peripheral | SFLOAT SpO2/pulse-rate Spot-Check Measurement indications and Features |
| [Gatt/Health/PulseOximeterClient](Gatt/Health/PulseOximeterClient/) | Central | Features read and SpO2/pulse-rate measurement decoding |
| [Gatt/Health/GlucoseServer](Gatt/Health/GlucoseServer/) | Peripheral | Record Access Control Point: RACP write → Measurement notify → RACP indicate |
| [Gatt/Health/GlucoseClient](Gatt/Health/GlucoseClient/) | Central | RACP report-records request and measurement/response decoding |
| [Gatt/Health/ContinuousGlucoseMonitoringServer](Gatt/Health/ContinuousGlucoseMonitoringServer/) | Peripheral | E2E-CRC-protected CGM Feature and CGM Measurement notifications |
| [Gatt/Health/ContinuousGlucoseMonitoringClient](Gatt/Health/ContinuousGlucoseMonitoringClient/) | Central | E2E-CRC verification and SFLOAT glucose/time-offset decoding |

### GATT — Fitness & cycling

| Example | Role | Description |
|---|---|---|
| [Gatt/Fitness/CyclingSpeedCadenceServer](Gatt/Fitness/CyclingSpeedCadenceServer/) | Peripheral | Multi-field wheel/crank CSC Measurement notifications, Feature, Sensor Location |
| [Gatt/Fitness/CyclingSpeedCadenceClient](Gatt/Fitness/CyclingSpeedCadenceClient/) | Central | Sensor Location read and CSC Measurement notification decoding |
| [Gatt/Fitness/RunningSpeedCadenceServer](Gatt/Fitness/RunningSpeedCadenceServer/) | Peripheral | Speed/cadence/stride/distance RSC Measurement notifications, Feature, Sensor Location |
| [Gatt/Fitness/RunningSpeedCadenceClient](Gatt/Fitness/RunningSpeedCadenceClient/) | Central | Sensor Location read and RSC Measurement notification decoding |
| [Gatt/Fitness/CyclingPowerServer](Gatt/Fitness/CyclingPowerServer/) | Peripheral | Signed 16-bit power Cycling Power Measurement notifications, Feature, Sensor Location |
| [Gatt/Fitness/CyclingPowerClient](Gatt/Fitness/CyclingPowerClient/) | Central | Sensor Location read and signed power measurement decoding |
| [Gatt/Fitness/FitnessMachineServer](Gatt/Fitness/FitnessMachineServer/) | Peripheral | Fitness Machine (FTMS) Indoor Bike Data notifications and Feature |
| [Gatt/Fitness/FitnessMachineClient](Gatt/Fitness/FitnessMachineClient/) | Central | Feature read and flags-driven Indoor Bike Data (speed/cadence/power) decoding |
| [Gatt/Fitness/LocationNavigationServer](Gatt/Fitness/LocationNavigationServer/) | Peripheral | Location and Speed notifications (speed + sint32 lat/lon) and LN Feature |
| [Gatt/Fitness/LocationNavigationClient](Gatt/Fitness/LocationNavigationClient/) | Central | LN Feature read and Location and Speed notification decoding |

### GATT — Alerts & proximity

| Example | Role | Description |
|---|---|---|
| [Gatt/Alerts/AlertNotificationServer](Gatt/Alerts/AlertNotificationServer/) | Peripheral | Category bitmask read, Control Point writes, New Alert notifications |
| [Gatt/Alerts/AlertNotificationClient](Gatt/Alerts/AlertNotificationClient/) | Central | Control Point "Notify New Alert Immediately" and New Alert decoding |
| [Gatt/Alerts/ImmediateAlertServer](Gatt/Alerts/ImmediateAlertServer/) | Peripheral | Find Me target: Alert Level Write Without Response handling |
| [Gatt/Alerts/ImmediateAlertClient](Gatt/Alerts/ImmediateAlertClient/) | Central | Find Me locator: raise/clear Alert Level via Write Without Response |
| [Gatt/Alerts/PhoneAlertStatusServer](Gatt/Alerts/PhoneAlertStatusServer/) | Peripheral | Alert Status / Ringer Setting notify, Ringer Control Point silent-mode |
| [Gatt/Alerts/PhoneAlertStatusClient](Gatt/Alerts/PhoneAlertStatusClient/) | Central | Read Alert Status, drive Ringer Control Point, decode Ringer Setting |
| [Gatt/Alerts/ProximityServer](Gatt/Alerts/ProximityServer/) | Peripheral | Proximity Reporter: Link Loss Alert Level + Tx Power (two services) |
| [Gatt/Alerts/ProximityClient](Gatt/Alerts/ProximityClient/) | Central | Proximity Monitor: read Tx Power, arm Link Loss Alert Level |

### HID over GATT

| Example | Role | Description |
|---|---|---|
| [Hid/KeyboardDevice](Hid/KeyboardDevice/) | HID Device | BLE keyboard typing via Serial commands, LED report reception |
| [Hid/KeyboardHost](Hid/KeyboardHost/) | HID Host | Connect to composite BLE HID, print every supported report type, write keyboard LEDs |
| [Hid/KeyboardNkro](Hid/KeyboardNkro/) | HID Device | N-key rollover keyboard (29-byte bitmap report) |
| [Hid/Mouse](Hid/Mouse/) | HID Device | Five-button relative mouse |
| [Hid/ConsumerControl](Hid/ConsumerControl/) | HID Device | Volume and play/pause media keys |
| [Hid/CompositeKeyboardMouse](Hid/CompositeKeyboardMouse/) | HID Device | One composite HID Service with keyboard and mouse reports |
| [Hid/VendorDevice](Hid/VendorDevice/) | HID Device | Report ID 6 Vendor Input / Output / Feature |
| [Hid/VendorHost](Hid/VendorHost/) | HID Host | Vendor Input reception and Output / Feature writes |
| [Hid/CustomDevice](Hid/CustomDevice/) | HID Device | Arbitrary Report Descriptor via `ble.hidCustom()` (input + output reports) |
| [Hid/CustomClient](Hid/CustomClient/) | GATT Client | Read a Custom HID's Report Map and decode its input report |

### MIDI

| Example | Role | Description |
|---|---|---|
| [Midi/MidiDevice](Midi/MidiDevice/) | MIDI Device | BLE MIDI peripheral: send Note On/Off, print received MIDI |
| [Midi/MidiHost](Midi/MidiHost/) | MIDI Host | BLE MIDI central: discover/subscribe and print MIDI, send notes |

### Security

| Example | Role | Description |
|---|---|---|
| [Security/JustWorksServer](Security/JustWorksServer/) | Peripheral | Encrypted characteristic with Just Works pairing + bonding |
| [Security/StaticPasskeyServer](Security/StaticPasskeyServer/) | Peripheral | MITM-authenticated characteristic with a static passkey (display side) |
| [Security/StaticPasskeyClient](Security/StaticPasskeyClient/) | Central | Passkey input side: `requestSecurity()` and authenticated reads |

### Diagnostics

| Example | Role | Description |
|---|---|---|
| [Info/ScanDump](Info/ScanDump/) | Diagnostics | Dump every advertisement field (UUIDs, manufacturer data, …) |
| [Info/ConnectionInspector](Info/ConnectionInspector/) | Diagnostics | Interactively connect and dump MTU, security state, bonds, counters |

## Suggested pairings on two boards

- Gap/Advertise ↔ Gap/Scan
- Gatt/Basics/Server ↔ Gatt/Basics/Client
- Gatt/Basics/NotifyServer ↔ Gatt/Basics/SubscribeClient / Gatt/Basics/AutoReconnectClient (and Gap/Mtu)
- Gatt/Basics/IndicateServer ↔ Gatt/Basics/IndicateClient
- Gatt/Basics/NusServer ↔ Gatt/Basics/NusClient
- Each `Gatt/<Category>/<Name>Server` ↔ its `…Client` (Device, Time, Sensors, Health, Fitness, Alerts)
- Security/StaticPasskeyServer ↔ Security/StaticPasskeyClient
- Hid/KeyboardDevice / Hid/CompositeKeyboardMouse / Hid/KeyboardNkro ↔ Hid/KeyboardHost
- Hid/VendorDevice ↔ Hid/VendorHost
- Hid/CustomDevice ↔ Hid/CustomClient
- Midi/MidiDevice ↔ Midi/MidiHost
- Info/ScanDump and Info/ConnectionInspector can observe anything — the other examples, smartphones, or commercial BLE devices
