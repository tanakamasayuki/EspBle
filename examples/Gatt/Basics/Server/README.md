# Server

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Registers a custom GATT service with one readable/writable characteristic and descriptor, then advertises it. The characteristic supports writes with and without response. It also carries one characteristic whose **value is produced at the moment it is read**.

Use the [Gatt/Client](../Client/) example on a second board (it targets the same UUIDs), or any GATT client app such as nRF Connect.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × GATT client (second board running Gatt/Client, or a smartphone app)

## What it does

- Adds service `10da4dd0-…`, characteristic `10da4dd1-…`, descriptor `10da4dd2-…`, and the read-only `10da4dd3-…` before `begin()`
- Sets the initial value to `ready`
- Prints each write received from a client, together with the connection ID
- Answers a read of `10da4dd3-…` with `millis()` taken at that moment
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

## Producing a value when it is read

Storing the value ahead of time with `setValue()` suits data this device already knows has changed. For a sensor-style value that should reflect the moment of the read, use `onRead()`.

```cpp
gattServer.onRead([](const EspBleGattReadRequest &request) {
  if (request.characteristic != liveCharacteristic) return;
  ble.gattServer().setValue(liveCharacteristic, String(millis()));
});
```

Whatever the callback passes to `setValue()` is what the peer receives. No periodic `setValue()` loop is needed, and **if nobody reads it, the work of producing the value never runs**.

**This one callback runs on the BLE stack task, not from `update()`.** The value has to exist before the ATT read response goes out, so there is nowhere to defer it to. Two consequences:

- **Keep it short.** Blocking here stalls the whole stack, and the peer sees the read time out. Avoid serial output inside it
- **It runs concurrently with `loop()`.** Unlike every other callback, touching shared state here needs synchronisation

## Key APIs

- `ble.gattServer().addService(uuid)` — register a service and return its handle; must be called before `begin()`
- `addCharacteristic(service, uuid, config)` — register a characteristic in that service and return its handle
- `EspBleGattCharacteristicConfig` — `readable`, `writable`, plus `notifiable`, `indicatable`, and encrypted/authenticated permissions
- `addDescriptor(characteristic, uuid, config)` / `EspBleGattDescriptorConfig` / `setDescriptorValue(descriptor, value)` — descriptor definition, permissions, and binary-safe value
- `gattServer.setValue(characteristic, value)` / `gattServer.value(characteristic, out)` — held value (binary-safe `String`, pointer+length overloads available)
- `gattServer.onWritten(callback)` — `EspBleGattWrite` with `connectionId`, the handle of the characteristic written, and the value
- `gattServer.onRead(callback)` — a read request; `EspBleGattReadRequest` carries `connectionId` and the target handle
- `gattServer.onDescriptorWritten(callback)` — `EspBleGattDescriptorWrite` with the descriptor handle and value

## Notes

- **One callback serves every characteristic.** With more than one registered, check `write.characteristic == myHandle`. The event also carries UUID strings, but those cannot tell apart characteristics that share a UUID, so comparing handles is the reliable test.
- **Several characteristics in one service may share a UUID.** The spec allows it and HID Reports are the everyday case. The attribute table is built through the BLE stack's own API, and both operations and events name their target by handle, so nothing is confused. The one exception is **automatic subscription restore after a reconnect, which keys on the UUID** — with duplicates it cannot say which one was subscribed, so re-subscribe by handle after reconnecting.
- **There is only one `onRead()`.** It cannot be multiplexed with `add*Listener()` like the other events, because only one place can own the decision of what value to return.
- **A value larger than the MTU is read in pieces.** When it does not fit one ATT response, the client asks for the rest (Read Long). The server only stores the value; the splitting is the stack's job.
- All registration must happen before `begin()`; `addService()` afterwards fails with `InvalidState`.

## Expected Serial output

```
Connection 1 wrote: hello from Central
Descriptor 10da4dd2-8eaa-4c69-9003-676174747277 wrote: descriptor value
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [Server.ino](Server.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [Client](../Client/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

Server - expose a custom GATT service with one readable/writable characteristic. All GATT server configuration must happen before begin() (the backend cannot add services after the server starts). Operate it from Gatt/Client or a generic GATT app.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `gattServer.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `gattServer.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `gattServer.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `gattServer.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
5. `gattServer.onRead(...)` — This synchronous GATT read callback runs on the BLE stack task. Produce the value quickly with setValue; do not wait, print extensively or perform slow device I/O here.
6. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
7. `advertising.addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

The writable stored value and on-demand live value have different paths. Keep onRead short on the stack task; use onWritten in update context for application work following a peer write.

Keep calling `ble.update()` from loop. The stack/host callbacks described above are exceptions: perform only the immediate response or bounded copy they require, rather than assuming they run from update.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
