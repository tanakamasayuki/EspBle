# EspBle Examples

> 日本語版: [README.ja.md](README.ja.md)

## The concepts are covered in the guide

How BLE works — the difference from Bluetooth Classic, GAP (finding and connecting), security (pairing and bonding), GATT (exchanging data), UUIDs, HID and BLE MIDI — is explained in the [BLE communication beginner guide](../docs/GUIDE_BLE_BASICS.md). Every term is defined there.

| What you want to know | Guide chapter | Matching examples |
|---|---|---|
| What BLE is, and how it differs from Classic | 1 | — |
| Advertising, scanning, connecting, addresses | 2 (GAP) | [Gap/](Gap/) |
| Pairing, bonding, authentication methods | 3 (Security) | [Security/](Security/) |
| Services, characteristics, read/write, notify | 4 (GATT) | [Gatt/](Gatt/) |
| Standard and custom UUID forms | 5 | — |
| Acting as a keyboard or mouse, or receiving their input | 6 (HID) | [Hid/](Hid/) |
| BLE MIDI instruments | 7 (BLE MIDI) | [Midi/](Midi/) |
| Selecting P4/C6 ESP-Hosted SDIO pins | ESP-Hosted setup | [Hosted/CustomPins](Hosted/CustomPins/) |
| Classic inquiry, SPP, HID and audio | [Classic communication guide](../docs/GUIDE_CLASSIC_BASICS.ja.md) (Japanese) | [Classic/](Classic/) |

Each example's README is written to stand on its own, so starting from an individual example without reading the guide works fine.

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
| [Hosted/CustomPins](Hosted/CustomPins/) | P4 Host | Override ESP-Hosted SDIO pins before `ble.begin()` when they differ from the board variant |
| [Hosted/WifiCoexistence](Hosted/WifiCoexistence/) | P4 Host | Wi-Fi and BLE over one shared ESP-Hosted transport, and the order they stop in |

### Bluetooth Classic (original ESP32, experimental)

Using `EspBleClassic` selects the separately built Classic host automatically.
There is no build flag: starting both `EspBle` and `EspBleClassic` is what makes
a sketch dual-host, and starting one is what makes it single-host.

**The two radios reach different peers, which is what decides between them.**
BLE HID (HOGP) is accepted by phones, tablets and PCs from roughly 2015 onwards.
Classic reaches what BLE cannot: older game consoles and PCs, car audio units,
headsets — and it is the only way to offer a serial port (SPP) or to carry audio
(A2DP, HFP). The HID examples come in pairs, one per radio, with the same calls
on both sides; pick the pair that matches the peer you have. Which to choose, and
what differs between the two where both exist, is in
[BLE and Classic](../docs/CLASSIC_VS_BLE.md).

| Example | Role | Description |
|---|---|---|
| [Classic/Inquiry](Classic/Inquiry/) | GAP | Device discovery: where an address comes from |
| [Classic/RadioSettings](Classic/RadioSettings/) | GAP | Transmit power, page timeout and minimum encryption key size |
| [Classic/SppServer](Classic/SppServer/) | SPP Server | Binary-safe SPP echo server |
| [Classic/SppClient](Classic/SppClient/) | SPP Client | Dial an address, resolve or name the RFCOMM channel |
| [Classic/SppStream](Classic/SppStream/) | SPP Server | SPP as an Arduino `Stream`, for code written against `Serial` |
| [Classic/SppPairing](Classic/SppPairing/) | SPP Server / GAP | Application-controlled pairing and bond management |
| [Classic/HidKeyboard](Classic/HidKeyboard/) | HID Device | Keyboard and mouse through the same profile API the BLE examples use |
| [Classic/HidMouse](Classic/HidMouse/) | HID Device | Motion, clicks, wheel and drag |
| [Classic/HidGamepad](Classic/HidGamepad/) | HID Device | Axes, hat and buttons — the case BLE cannot replace |
| [Classic/HidConsumerControl](Classic/HidConsumerControl/) | HID Device | Media keys and system requests |
| [Classic/HidKeyboardNkro](Classic/HidKeyboardNkro/) | HID Device | N-key rollover, with no six-key limit |
| [Classic/HidComposite](Classic/HidComposite/) | HID Device | Keyboard, mouse and media keys in one device, and the SDP record limit on how many fit |
| [Classic/HidKeyboardHost](Classic/HidKeyboardHost/) | HID Host | Decoded key and mouse events from the peer's Report Descriptor |
| [Classic/HidVendorDevice](Classic/HidVendorDevice/) | HID Device | Classic HID Device with an arbitrary Report Descriptor |
| [Classic/HidVendorHost](Classic/HidVendorHost/) | HID Host | Connect by address and receive raw Input Reports |
| [Classic/A2dpSinkRaw](Classic/A2dpSinkRaw/) | A2DP Sink | Receive codec configuration and encoded SBC media callbacks |
| [Classic/A2dpSource](Classic/A2dpSource/) | A2DP Source | Send encoded SBC frames with backpressure |
| [Classic/A2dpSinkAvrcp](Classic/A2dpSinkAvrcp/) | A2DP Sink / AVRCP TG | A2DP connection, playback control, and absolute volume |
| [Classic/AvrcpController](Classic/AvrcpController/) | AVRCP CT | Press play on another device, ask for status and metadata |
| [Classic/HfpClientRaw](Classic/HfpClientRaw/) | HFP Client | Single-call control and raw CVSD/mSBC SCO transport |
| [Classic/HfpAudioGatewayRaw](Classic/HfpAudioGatewayRaw/) | HFP Audio Gateway | Small telephony model and raw CVSD/mSBC SCO transport |

### GAP — advertise, scan, connect

| Example | Role | Description |
|---|---|---|
| [Gap/Advertise](Gap/Advertise/) | Peripheral | Connectable legacy advertising with name + service UUID |
| [Gap/Scan](Gap/Scan/) | Central | Continuous active scan printing address / RSSI / name |
| [Gap/Connect](Gap/Connect/) | Central | Scan for a service UUID and connect; async connect/disconnect/failure events |
| [Gap/Mtu](Gap/Mtu/) | Central | Preferred-MTU exchange and notification payload limits |
| [Gap/ConnectionParameters](Gap/ConnectionParameters/) | Central | Change interval / latency / timeout and the PHY after connecting |
| [Gap/Beacon](Gap/Beacon/) | Broadcaster | Non-connectable, non-scannable beacon with manufacturer data and interval control |
| [Gap/IBeacon](Gap/IBeacon/) | Broadcaster | Broadcast an Apple iBeacon (UUID / major / minor / measured power) |
| [Gap/ServiceData](Gap/ServiceData/) | Broadcaster | Broadcast a temperature as Service Data (AD 0x16); publish values without a connection |
| [Gap/ScanResponse](Gap/ScanResponse/) | Peripheral | Split the payload across advertising data and scan response to get past the 31-byte limit |
| [Gap/AcceptList](Gap/AcceptList/) | Peripheral | Restrict who may connect with the Filter Accept List |
| [Gap/DirectedAdvertise](Gap/DirectedAdvertise/) | Peripheral | Directed advertising aimed at a single peer; carries no payload |
| [Gap/PrivateAddress](Gap/PrivateAddress/) | Peripheral | Advertise with a random static / resolvable private address |
| [Gap/MultiConnection](Gap/MultiConnection/) | Central | Hold several peripheral connections at once, each named by its own ID |

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
| [Hid/Gamepad](Hid/Gamepad/) | HID Device | Six axes, a hat switch and 32 buttons |
| [Hid/CompositeKeyboardMouse](Hid/CompositeKeyboardMouse/) | HID Device | One composite HID Service with keyboard and mouse reports |
| [Hid/VendorDevice](Hid/VendorDevice/) | HID Device | Report ID 6 Vendor Input / Output / Feature |
| [Hid/VendorHost](Hid/VendorHost/) | HID Host | Vendor Input reception and Output / Feature writes |
| [Hid/CustomDevice](Hid/CustomDevice/) | HID Device | Arbitrary Report Descriptor via `ble.hidCustom()` (input + output reports) |
| [Hid/CustomClient](Hid/CustomClient/) | GATT Client | Read a Custom HID's Report Map, take each report's role from its Report Reference by handle, and decode the input report |

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
| [Security/RuntimePasskeyServer](Security/RuntimePasskeyServer/) | Peripheral | Display side of a passkey generated per pairing |
| [Security/RuntimePasskeyClient](Security/RuntimePasskeyClient/) | Central | Input side, supplying the passkey at runtime with `providePasskey()` |
| [Security/NumericComparisonServer](Security/NumericComparisonServer/) | Peripheral | Pairing by confirming the 6 digits shown on both sides (peripheral) |
| [Security/NumericComparisonClient](Security/NumericComparisonClient/) | Central | The central half of the same |

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
- Security/RuntimePasskeyServer ↔ Security/RuntimePasskeyClient
- Security/NumericComparisonServer ↔ Security/NumericComparisonClient
- Hid/KeyboardDevice / Hid/CompositeKeyboardMouse / Hid/KeyboardNkro ↔ Hid/KeyboardHost
- Hid/VendorDevice ↔ Hid/VendorHost
- Hid/CustomDevice ↔ Hid/CustomClient
- Midi/MidiDevice ↔ Midi/MidiHost
- Info/ScanDump and Info/ConnectionInspector can observe anything — the other examples, smartphones, or commercial BLE devices
