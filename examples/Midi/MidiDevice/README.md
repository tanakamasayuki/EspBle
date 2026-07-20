# MidiDevice

> 日本語版: [README.ja.md](README.ja.md)

Turns the board into a BLE MIDI peripheral. Pair it from a phone/tablet DAW or the [MidiHost](../MidiHost/) example, then send notes from Serial and print any MIDI the host sends back.

## Hardware

- 1 × ESP32-S3 running this sketch (MIDI device / peripheral)
- 1 × BLE MIDI host: a smartphone/tablet DAW, a computer, or a second board running the [MidiHost](../MidiHost/) example

## What it does

- Registers the BLE MIDI service and its I/O characteristic before `begin()` (the service UUID is added to advertising)
- Sends middle-C Note On then Note Off on `n`
- Prints MIDI received from the host (host → device)

## Serial commands

| Command | Action |
|---------|--------|
| `n` | Send Note On (middle C) then Note Off |

## Key APIs

- `EspBleMidiDevice midi(ble)` — construct with a reference (like `EspUsbDeviceMidi(device)`)
- `midi.begin()` — register the service; call before `ble.begin()`
- `midi.noteOn/noteOff/controlChange/programChange/polyPressure/channelPressure/pitchBend(...)`
- `midi.onMessage(callback)` — MIDI received from the host, decoded into `EspBleMidiMessage`
- `midi.ready()` — true while a host is subscribed

## Expected Serial output

```
MIDI in: status=0xb0 data1=7 data2=100 ts=1234
```
