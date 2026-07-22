# MidiHost

> 日本語版: [README.ja.md](README.ja.md)

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
