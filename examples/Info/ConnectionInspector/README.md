# ConnectionInspector

> 日本語版: [README.ja.md](README.ja.md)

Interactive diagnostic tool. It lists nearby connectable devices with index numbers, connects to the one you pick, and dumps the full connection snapshot: connection ID, backend handle, peer address and type, local role, negotiated MTU (and the resulting notification payload limit), and the security state (encrypted / authenticated / bonded / key size). It can also dump the bond store and the library's diagnostic counters.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- Nearby BLE peripherals to inspect (any advertising device)

## What it does

- Scans and lists up to 10 unique connectable devices as `[index] address rssi name`
- `0`–`9` connects to the listed device with that index and prints its connection snapshot
- `s` clears the list and rescans; `d` disconnects the current connection; `b` dumps the bond store; `q` prints the diagnostic counters
- Security is disabled here, so peripherals that require encryption still accept the connection and show their link info, but reject attribute access

## Key APIs

- `EspBleConnection` — `id`, `handle`, `peerAddress`, `peerAddressType`, `localRole`, `mtu`, `maximumNotificationPayload()`, `encrypted`, `authenticated`, `bonded`, `encryptionKeySize`
- `ble.connect(scanResult)` / `ble.disconnect(connectionId)` / `ble.onConnectionFailed(callback)`
- `ble.bondCount()` / `ble.bond(index, out)` — snapshot access to the bond store
- `ble.connectionCount()`, `ble.droppedEventCount()`, `ble.scanner().droppedResultCount()`

## Expected Serial output

```
Commands: 0-9 connect to listed device, s rescan, d disconnect, b bonds, q counters
SCAN restart success=1 - send the list number to connect
[0] 5a:b8:1e:0c:2f:71 rssi=-52 name=EspBle Keyboard
CONNECT [0] 5a:b8:1e:0c:2f:71 accepted=1
CONNECTION id=1 handle=0 peer=5a:b8:1e:0c:2f:71(type=0) role=Central
  mtu=255 maxNotificationPayload=252
  encrypted=0 authenticated=0 bonded=0 keySize=0
```
