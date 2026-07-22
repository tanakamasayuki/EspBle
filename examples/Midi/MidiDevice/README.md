# MidiDevice

> 日本語版: [README.ja.md](README.ja.md)

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
