# MidiHost

> 日本語版: [README.ja.md](README.ja.md)

Connects to a BLE MIDI peripheral as a central: scan → connect → discover → subscribe → print decoded MIDI. Send a note from Serial. Works with the [MidiDevice](../MidiDevice/) example or a commercial BLE MIDI instrument.

## Hardware

- 1 × ESP32-S3 running this sketch (MIDI host / central)
- 1 × BLE MIDI peripheral: the [MidiDevice](../MidiDevice/) example or a commercial BLE MIDI instrument

## What it does

- Scans for the BLE MIDI service and connects to the first connectable match
- Discovers and subscribes to the MIDI I/O characteristic on connect
- Prints decoded MIDI (`onMidiMessage`, mirroring EspUsbHost)
- Sends middle-C Note On/Off on `n`

## Serial commands

| Command | Action |
|---------|--------|
| `n` | Send Note On (middle C) then Note Off to the device |

## Key APIs

- `EspBleMidiHost midi(ble)` — construct with a reference
- `midi.begin()` — install host GATT callbacks; call after `ble.begin()`
- `midi.discover(connectionId)` — discover and subscribe (call after connect / security)
- `midi.onMidiMessage(callback)` — decoded `EspBleMidiMessage` (status/data1/data2/timestamp)
- `midi.sendNoteOn/sendNoteOff/sendControlChange/sendProgramChange(connectionId, ...)`
- `midi.sendSysEx(connectionId, data, length)` — send a framed SysEx; large messages are split across packets automatically

## Expected Serial output

```
MIDI: conn=1 status=0x90 data1=60 data2=100 ts=165
```
