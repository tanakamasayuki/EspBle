# Coming to EspBle from another library

> 日本語版: [GUIDE_MIGRATION.ja.md](GUIDE_MIGRATION.ja.md)

If you already have a sketch built on Arduino-ESP32's bundled `BLEDevice`
classes, on NimBLE-Arduino, or on `BluetoothSerial`, this document maps what you
know onto EspBle. It is a translation table, not a tutorial: for what each
concept means, see the [BLE guide](GUIDE_BLE_BASICS.md) and the
[Classic guide](GUIDE_CLASSIC_BASICS.md); for behaviour under load, see
[EspBle in depth](GUIDE_ADVANCED.md).

## 1. Three structural differences

Everything else is renaming. These three change the shape of a sketch, so read
them before translating call by call.

**1. You must call `update()`.** EspBle delivers every event from
`EspBle::update()` (and `EspBleClassic::update()`) on your own task, so `loop()`
has to call it. Libraries that dispatch from the stack task do not need this; a
port that forgets it looks like a radio that connects and then goes silent.

```cpp
void loop() {
  ble.update();      // required
  // your code
}
```

**2. Callbacks are values, not subclasses.** Instead of deriving from a callback
class and handing over an instance, you assign a `std::function`. Lambdas and
captures work, there is nothing to `new`, and several observers can watch one
event (`addXListener()`, four per event plus the primary).

```cpp
ble.onConnected([](const EspBleConnection &connection) { /* ... */ });
```

**3. The GATT database is built before `begin()`.** Services, characteristics and
descriptors are registered first; `begin()` finalizes and starts the database. The
backend cannot add a service afterwards, so a sketch that creates services in
response to a connection has to be restructured. Handles come back as small value
types (`EspBleGattService`, `EspBleGattCharacteristic`) that you keep and pass
back in, rather than pointers you own.

## 2. From the core's bundled BLE wrapper

Arduino-ESP32 ships `BLEDevice` / `BLEServer` / `BLEClient` (Bluedroid on the
original ESP32, NimBLE elsewhere). EspBle replaces the whole wrapper — do not mix
them in one sketch.

| Bundled wrapper | EspBle |
|---|---|
| `BLEDevice::init("name")` | `EspBleConfig::deviceName`, then `ble.begin(config)` |
| `BLEDevice::deinit()` | `ble.end()` |
| `BLEDevice::createServer()` | `ble.gattServer()` — one server, always present |
| `server->createService(uuid)` | `gattServer().addService(uuid)` before `begin()` |
| `service->createCharacteristic(uuid, PROPERTY_READ \| PROPERTY_NOTIFY)` | `gattServer().addCharacteristic(service, uuid, config)` with `EspBleGattCharacteristicConfig` fields (`readable`, `writable`, `writableWithoutResponse`, notify/indicate) |
| `new BLE2902()` for the CCCD | Nothing to add. Subscription state arrives through `onSubscriptionChanged()` |
| `characteristic->setValue(...)` | `gattServer().setValue(characteristic, value)` |
| `characteristic->notify()` | `gattServer().notify(characteristic, value)` (`indicate()` for indications) |
| `setCallbacks(new MyCharacteristicCallbacks())` | `gattServer().onWritten(...)`, `onRead(...)`, `onDescriptorWritten(...)`, `onSubscriptionChanged(...)`, `onSent(...)` |
| `BLEDevice::getAdvertising()` and its start/stop | `ble.advertising()` |
| `BLEDevice::getScan()`, `setActiveScan()`, `start(seconds)` | `ble.scanner()` with `EspBleScanConfig`; results arrive through the scan callback from `update()` |
| `BLEClient` per peer, `connect(address)` | `ble.connect(scanResult)` or `ble.connect(address, addressType)`; peers are identified by a connection id |
| `client->getService(uuid)->getCharacteristic(uuid)->readValue()` | `ble.readCharacteristic(connectionId, serviceUuid, characteristicUuid)` — asynchronous, answered in its callback, or addressed by attribute handle |
| `characteristic->registerForNotify(cb)` | `ble.subscribe(...)`, with notifications delivered to the notification callback |
| `BLESecurity`, `setStaticPIN()` | `EspBleConfig::security` (IO capability, bonding, MITM), `providePasskey()`, `confirmNumericComparison()` |

The behavioural differences that bite during a port:

- **Reads and writes are asynchronous.** There is no `readValue()` that returns a
  string. You get a result in a callback, and only one GATT operation is in
  flight at a time (eight more may queue). A `for` loop that reads ten
  characteristics has to become a chain.
- **One connection id, not a client object per peer.** Several simultaneous
  connections share one `EspBle`, each with its own cache and subscriptions.
- **Service discovery is explicit** (`discover()`), and its results are reported
  rather than walked as a tree of pointers.

## 3. From NimBLE-Arduino

NimBLE-Arduino (`NimBLEDevice`, `NimBLEServer`, …) is a much closer fit: it is the
same host EspBle talks to, so the concepts line up and mainly the ownership model
differs.

| NimBLE-Arduino | EspBle |
|---|---|
| `NimBLEDevice::init("name")` | `ble.begin(config)` |
| `NimBLEServer`, `NimBLEService`, `NimBLECharacteristic` pointers | value handles from `gattServer().addService()` / `addCharacteristic()` |
| `NimBLECharacteristicCallbacks` subclass | `onWritten()` / `onRead()` / `onSubscriptionChanged()` on the server |
| `NimBLEClient::connect()`, `getService()`, `getCharacteristic()` | `ble.connect()`, `discover()`, then UUID- or handle-addressed operations |
| `NimBLEAdvertising` | `ble.advertising()` |
| Manual `NimBLEDevice::setMTU()` | `EspBleConfig::preferredMtu` (247 by default), live value through `onMtuChanged()` |
| Stack-task callbacks | `update()`-dispatched callbacks, with four documented exceptions ([in depth §1](GUIDE_ADVANCED.md#1-where-your-callbacks-run)) |

What EspBle adds on top: multiple simultaneous connections with per-connection
caches, persistent subscriptions restored after reconnection, auto-reconnect,
composable HID device and host profiles, BLE MIDI, and — on the original ESP32 —
Bluetooth Classic and the option of running both radios at once.

What EspBle does not expose: raw `ble_gap_*` / `ble_gattc_*` calls. If your sketch
depends on driving NimBLE directly, EspBle is not a drop-in replacement.

## 4. From `BluetoothSerial` (Classic SPP)

`BluetoothSerial` is the classic "Bluetooth as a serial port" API. EspBle covers
the same ground with `EspBleClassic` plus SPP, and
[`Classic/SppStream`](../examples/Classic/SppStream/) is the example to start
from.

| `BluetoothSerial` | EspBle |
|---|---|
| `SerialBT.begin("name")` (device) | `EspBleClassicConfig::deviceName`, `classic.begin(config)`, then `classic.spp().startServer()` |
| `SerialBT.begin("name", true)` (master) | `classic.spp().connect(address)` or `connectToChannel(address, channel)` |
| `SerialBT.available()` / `read()` / `write()` / `print()` | An `EspBleClassicSppStream` attached to the session — it is an Arduino `Stream`, so `readStringUntil()` and `parseInt()` work too |
| `SerialBT.hasClient()` | `spp().onConnected()` / `onDisconnected()` session events, or `spp().sessionCount()` |
| `SerialBT.setPin()` | Legacy PIN pairing is refused deliberately. Configure `EspBleClassicSecurityConfig` and answer passkey or numeric-comparison events |
| `SerialBT.discover()` | `classic.inquiry()` — address, name, Class of Device, RSSI, plus `requestServices()` / `requestName()` |
| `SerialBT.end()` | `classic.end()` |

Differences worth knowing before you port:

- **One `write()` becomes one SPP packet** (up to 990 bytes), so printing a line
  at a time is much cheaper than a character at a time.
- **The outgoing queue is finite** (eight writes). `Stream::write()` waits up to
  `setWriteTimeout()` — 1000 ms by default, 0 to never wait — and returns how much
  it took, which is what a `Serial` with a full buffer does.
- **You still call `classic.update()`** in `loop()`; session events arrive there.
- **A device can publish several SPP services** (four), each on its own RFCOMM
  channel, which is why the client side has `connectToChannel()`.
- **`0x00` is data**, not a terminator: the stream is binary-safe in both
  directions.

## 5. From an audio library

Libraries like the popular A2DP sink implementations hand you PCM and drive I2S.
EspBle deliberately stops one layer earlier: `a2dpSink()` reports
**already-encoded** SBC frames, and HFP reports raw SCO frames. Decoding, PCM
processing and device I/O belong to a separate library on top (that boundary is
what keeps the Bluetooth side testable on its own).

So a port looks like this: EspBle gives you the codec configuration
(`onCodecConfigured()`) and the encoded payloads (`onMedia()`, valid only inside
the callback — copy what you keep), and your decoder produces PCM. AVRCP control
(play/pause keys, absolute volume) and A2DP delay reporting are available through
`avrcp()` and `setDelay()`.

If you want PCM without writing a decoder, use the released
[PCMFlowBluetooth](https://github.com/tanakamasayuki/PCMFlowBluetooth) library:
EspBle remains the transport and PCMFlowBluetooth supplies the SBC codec and PCM
boundary. Its repository includes receive/playback, transmission, PCM diagnostic,
and PCMFlow integration examples.

## 6. Things with no equivalent

Do not plan a port around these:

- **No raw HCI, and no raw NimBLE or Bluedroid API.** On the original ESP32 the
  HCI broker's opcode classification is what makes running both radios safe.
- **No SPP over VFS** (`esp_spp_vfs_register()`), because
  `EspBleClassicSppStream` covers the same need without a file-descriptor path.
- **No codecs, PCM or device I/O**, as above.
- **No BLE audio (LE Audio).**
- **Extended and periodic advertising, LE 2M and LE Coded PHY are unavailable on
  the original ESP32** — that chip has a BLE 4.2-class controller, and the
  bundled host is built with extended advertising compiled out. They work on the
  native-controller targets where the core's host provides them.
- **One HID Host device at a time on Classic**, and one HID Host connection.

The full picture of what exists, what is verified and what is not is in the
[Feature Matrix](FEATURE_MATRIX.md) and, for Classic,
[CLASSIC_FEATURE_INVENTORY.ja.md](CLASSIC_FEATURE_INVENTORY.ja.md) (Japanese).
