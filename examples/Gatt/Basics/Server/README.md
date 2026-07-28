# Server

> 日本語版: [README.ja.md](README.ja.md)

Registers a custom GATT service with one readable/writable characteristic and descriptor, then advertises it. The characteristic supports writes with and without response.

Use the [Gatt/Client](../Client/) example on a second board (it targets the same UUIDs), or any GATT client app such as nRF Connect.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × GATT client (second board running Gatt/Client, or a smartphone app)

## What it does

- Adds service `10da4dd0-…`, characteristic `10da4dd1-…`, and descriptor `10da4dd2-…` before `begin()`
- Sets the initial value to `ready`
- Prints each write received from a client, together with the connection ID
- Advertises the service UUID so clients can find it

## Building the server from handles

Registration is a **three-step handle chain**: the handle from `addService()` goes into `addCharacteristic()`, whose handle goes into `addDescriptor()`.

```cpp
const EspBleGattService service = gattServer.addService(SERVICE_UUID);
characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
descriptor = gattServer.addDescriptor(characteristic, DESCRIPTOR_UUID, descriptorConfig);
```

Every later value, send, and event check uses those handles rather than UUIDs, because **a UUID is a type, not an identity**. The spec lets one device expose several services with the same UUID, and from the client side a peer with several same-UUID characteristics (HID Reports, for example) is entirely normal.

Keep the handles in globals. A failed registration returns an invalid handle, which `valid()` reports.

## Key APIs

- `ble.gattServer().addService(uuid)` — register a service and return its handle; must be called before `begin()`
- `addCharacteristic(service, uuid, config)` — register a characteristic in that service and return its handle
- `EspBleGattCharacteristicConfig` — `readable`, `writable`, plus `notifiable`, `indicatable`, and encrypted/authenticated permissions
- `addDescriptor(characteristic, uuid, config)` / `EspBleGattDescriptorConfig` / `setDescriptorValue(descriptor, value)` — descriptor definition, permissions, and binary-safe value
- `gattServer.setValue(characteristic, value)` / `gattServer.value(characteristic, out)` — held value (binary-safe `String`, pointer+length overloads available)
- `gattServer.onWritten(callback)` — `EspBleGattWrite` with `connectionId`, the handle of the characteristic written, and the value
- `gattServer.onDescriptorWritten(callback)` — `EspBleGattDescriptorWrite` with the descriptor handle and value

## Notes

- **One callback serves every characteristic.** With more than one registered, check `write.characteristic == myHandle`. The event also carries UUID strings, but those cannot tell apart characteristics that share a UUID, so comparing handles is the reliable test.
- **Two characteristics in one service may not share a UUID.** The bundled backend reuses the existing entry instead of registering the second, so `addCharacteristic()` refuses it and returns an invalid handle rather than leaving sends silently undelivered.
- All registration must happen before `begin()`; `addService()` afterwards fails with `InvalidState`.

## Expected Serial output

```
Connection 1 wrote: hello from Central
Descriptor 10da4dd2-8eaa-4c69-9003-676174747277 wrote: descriptor value
```
