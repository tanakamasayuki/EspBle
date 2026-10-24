# ProximityServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

The Proximity profile **Reporter** role. Hosts two standard services at once: the Link Loss Service (0x1803) with a read/write Alert Level (0x2A06), and the Tx Power Service (0x1804) with a read-only signed-int8 Tx Power Level (0x2A07).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / Proximity Reporter)
- 1 × central: the [ProximityClient](../ProximityClient/) example, or any Proximity Monitor

## What it does

- Registers both services before `begin()` and advertises 0x1803
- Exposes a fixed Tx Power Level of -8 dBm
- Stores the written Link Loss Alert Level in `onWritten` so it can be read back (a real Reporter would alert on link loss according to this level)

## Key APIs

- Two `addService(...)` calls on one server
- `ble.gattServer().onWritten(...)` — receive Alert Level writes

## Expected Serial output

```
Link Loss Alert Level: 2 (High Alert)
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [ProximityServer.ino](ProximityServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [ProximityClient](../ProximityClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

ProximityServer - the Proximity profile Reporter role. It hosts two standard services at once: the Link Loss Service (0x1803) with a read/write Alert Level (0x2A06), and the Tx Power Service (0x1804) with a read-only signed-int8 Tx Power Level (0x2A07). A real Reporter alerts on link loss according to the configured Alert Level.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `server.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
5. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
6. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Tx Power is a declared radio level and Link Loss Alert Level is configuration. They do not provide a calibrated distance measurement; implement the physical link-loss action separately if needed.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
