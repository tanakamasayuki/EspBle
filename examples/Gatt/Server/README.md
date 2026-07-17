# Server

> 日本語版: [README.ja.md](README.ja.md)

Registers a custom GATT service with one readable/writable characteristic, then advertises it. All GATT server configuration must happen before `begin()` — the bundled backend cannot add services after the server starts.

Use the [Gatt/Client](../Client/) example on a second board (it targets the same UUIDs), or any GATT client app such as nRF Connect.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × GATT client (second board running Gatt/Client, or a smartphone app)

## What it does

- Adds service `10da4dd0-…` with characteristic `10da4dd1-…` (readable and writable) before `begin()`
- Sets the initial value to `ready`
- Prints each write received from a client, together with the connection ID
- Advertises the service UUID so clients can find it

## Key APIs

- `ble.gattServer().addService(uuid)` / `addCharacteristic(serviceUuid, characteristicUuid, config)` — must be called before `begin()`
- `EspBleGattCharacteristicConfig` — `readable`, `writable`, plus `notifiable`, `indicatable`, and encrypted/authenticated permissions
- `gattServer.setValue(...)` / `gattServer.value(...)` — held value (binary-safe `String`, pointer+length overloads available)
- `gattServer.onWritten(callback)` — `EspBleGattWrite` with `connectionId`, UUIDs, and the written value

## Expected Serial output

```
Connection 1 wrote: hello from Central
```
