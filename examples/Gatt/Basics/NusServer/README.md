# NusServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Implements the Nordic UART Service (NUS) UUID layout with the generic GATT server API (peripheral): RX (`6e400002-…`) accepts writes and TX (`6e400003-…`) sends notifications, under service `6e400001-…`. Received RX data is echoed back to subscribed TX clients.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × ESP32-S3 running the [NusClient](../NusClient/) example (or any NUS-compatible central)

## What it does

- Registers the NUS service with a writable RX characteristic (with and without response) and a notifiable TX characteristic before `begin()`
- Prints each RX write, then echoes the same bytes back as a TX notification and prints whether the echo was accepted
- Reports TX subscription changes via `onSubscriptionChanged()`
- Advertises the NUS service UUID under the name `EspBle NUS`

## Key APIs

- `gattServer.addCharacteristic(service, uuid, config)` — RX with `writable` + `writableWithoutResponse`, TX with `notifiable`; the returned handle drives everything afterwards
- `gattServer.onWritten(callback)` — `EspBleGattWrite` filtered to the RX UUID via `characteristicUuid.equalsIgnoreCase(...)`
- `gattServer.notify(characteristic, value)` — echoes the received bytes on TX; returns whether it was accepted
- `gattServer.onSubscriptionChanged(callback)` — `subscription.notifications` for the TX characteristic

## Notes

- NUS is a packet-oriented GATT convention, not Bluetooth Classic SPP. Payloads must fit the connection's ATT/MTU limit; add your own framing for messages that span multiple packets.

## Expected Serial output

```
TX notifications: 1
RX: hello
Echo accepted: 1
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [NusServer.ino](NusServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [NusClient](../NusClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
4. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.
5. `server.onSubscriptionChanged(...)` — Track which connection subscribed before sending it updates; connecting does not automatically enable characteristic notifications.
6. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
7. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

NUS is a pair of characteristics, not an automatic Serial stream. This example uses lines from Serial; design framing and chunking explicitly for binary or long messages, and treat subscription success as readiness.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
