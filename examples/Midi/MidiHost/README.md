# MidiHost

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 7, "BLE MIDI"

Connects to a BLE MIDI peripheral as a central: scan the BLE MIDI service → connect → discover → subscribe → print decoded MIDI. Send a note from Serial. Pairs with the [MidiDevice](../MidiDevice/) example or a commercial BLE MIDI instrument.

## Hardware

- 1 × ESP32-S3 running this sketch (MIDI host / central)
- 1 × BLE MIDI peripheral: the [MidiDevice](../MidiDevice/) example or a commercial BLE MIDI instrument

## What it does

- Scans for the BLE MIDI service and connects to the first connectable match
- Discovers and subscribes to the MIDI I/O characteristic immediately on connect (no security in this example)
- Prints decoded MIDI, including SysEx chunks
- Sends middle-C Note On then Note Off on Serial command `n` (only once connected and subscribed)

## Key APIs

- `EspBleMidiHost midi(ble)` — construct with a reference to the `EspBle` instance
- `midi.begin()` — install host GATT callbacks; call after `ble.begin()`
- `midi.discover(connectionId)` — discover and subscribe (call after connect / security)
- `midi.onMidiMessage(callback)` — decoded `EspBleMidiMessage` (status / data1 / data2 / timestamp)
- `midi.sendNoteOn(connectionId, ...)` / `midi.sendNoteOff(connectionId, ...)` — send to the device
- `midi.ready(connectionId)` — true once subscribed

## Expected Serial output

```
MIDI: conn=1 status=0x90 data1=60 data2=100 ts=165
```

## Related guides

- [BLE guide §7 BLE MIDI](../../../docs/GUIDE_BLE_BASICS.md#7-ble-midi--connecting-as-an-instrument) — the packet format and timestamps

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [MidiHost.ino](MidiHost.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

MidiHost - connect to a BLE MIDI peripheral as a central: scan the MIDI service -> connect -> discover -> subscribe -> print MIDI messages. Send a note from serial input. Pairs with the MidiDevice example or a commercial BLE MIDI instrument.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
4. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
5. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

BLE MIDI carries performance events, not sampled audio. Select a MIDI input/output in the receiving application; pair Note On with Note Off and choose channels, notes and velocities in the API format. This example sends the off event immediately after the on event, so add nonblocking scheduling if you need an audible sustained note.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
