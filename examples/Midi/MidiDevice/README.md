# MidiDevice

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 7, "BLE MIDI"

Advertises a BLE MIDI peripheral using the standard BLE MIDI service. Send Note On/Off from Serial and print any MIDI a connected host sends back. Pairs with the [MidiHost](../MidiHost/) example or any BLE MIDI host (phone/tablet DAW).

## Hardware

- 1 × ESP32-S3 running this sketch (MIDI device / peripheral)
- 1 × BLE MIDI host: a smartphone/tablet DAW, a computer, or a second board running the [MidiHost](../MidiHost/) example

## What it does

- Registers the BLE MIDI service and its I/O characteristic before `begin()` (the service UUID is added to advertising)
- Sends middle-C Note On then Note Off on Serial command `n` (only while a host is subscribed)
- Prints MIDI received from the host (host → device), including SysEx chunks

## Key APIs

- `EspBleMidiDevice midi(ble)` — construct with a reference to the `EspBle` instance
- `midi.begin()` — register the service; call before `ble.begin()`
- `midi.noteOn(channel, note, velocity)` / `midi.noteOff(...)` — send channel-voice messages
- `midi.onMessage(callback)` — MIDI received from the host, decoded into `EspBleMidiMessage`
- `midi.ready()` — true while a host is subscribed

## Expected Serial output

```
MIDI in: status=0xb0 data1=7 data2=100 ts=1234
SysEx chunk: start=1 end=0 length=16
```

## Related guides

- [BLE guide §7 BLE MIDI](../../../docs/GUIDE_BLE_BASICS.md#7-ble-midi--connecting-as-an-instrument) — the packet format and timestamps

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [MidiDevice.ino](MidiDevice.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

MidiDevice - advertise a BLE MIDI peripheral. Send Note On/Off from serial input and print any MIDI a connected host sends back. Pairs with the MidiHost example or any BLE MIDI host (e.g. a phone/tablet DAW).

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `midi.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.

## Verify and adapt

BLE MIDI carries performance events, not sampled audio. Select a MIDI input/output in the receiving application; pair Note On with Note Off and choose channels, notes and velocities in the API format. This example sends the off event immediately after the on event, so add nonblocking scheduling if you need an audible sustained note.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
