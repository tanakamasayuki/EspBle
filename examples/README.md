# EspBle Examples

> 日本語版: [README.ja.md](README.ja.md)

Every example ships with a `sketch.yaml` profile pinned to the verified Arduino-ESP32 version, so it can be built without IDE board setup:

```sh
arduino-cli compile --profile esp32s3 examples/<path>
```

Examples that need a counterpart list it in their own README. Any pair can also be exercised against a smartphone app such as nRF Connect.

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
| [Security/JustWorksServer](Security/JustWorksServer/) | Peripheral | Encrypted characteristic with Just Works pairing + bonding |
| [Security/StaticPasskeyServer](Security/StaticPasskeyServer/) | Peripheral | MITM-authenticated characteristic with a static passkey |
| [Hid/KeyboardDevice](Hid/KeyboardDevice/) | HID Device | BLE keyboard typing via Serial commands, LED report reception |
| [Hid/KeyboardHost](Hid/KeyboardHost/) | HID Host | Connect to a BLE keyboard, print keys, write LEDs |

Suggested pairings on two boards:

- Gap/Advertise ↔ Gap/Scan
- Gatt/Server ↔ Gatt/Client
- Gatt/NotifyServer ↔ Gatt/SubscribeClient (and Gap/Mtu)
- Hid/KeyboardDevice ↔ Hid/KeyboardHost
