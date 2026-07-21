# MidiHost

> English: [README.md](README.md)

CentralとしてBLE MIDI Peripheralへ接続します。scan → 接続 → Discovery → 購読 → デコード済みMIDI表示。Serialでノートを送信します。[MidiDevice](../MidiDevice/) exampleや市販BLE MIDI楽器と接続できます。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。MIDI Host / Central）
- 1 × BLE MIDI Peripheral: [MidiDevice](../MidiDevice/) exampleまたは市販BLE MIDI楽器

## 動作

- BLE MIDI Serviceをscanし、最初のconnectableな相手へ接続します
- 接続時にMIDI I/O characteristicをDiscovery・購読します
- デコード済みMIDIを表示します（`onMidiMessage`、EspUsbHostに対応）
- `n`で中央ハのNote On/Offを送信します

## Serialコマンド

| コマンド | 動作 |
|---------|------|
| `n` | Deviceへ Note On（中央ハ）→ Note Off送信 |

## 主なAPI

- `EspBleMidiHost midi(ble)` — 参照で構築
- `midi.begin()` — HostのGATTコールバックを設定。`ble.begin()`の後に呼ぶ
- `midi.discover(connectionId)` — Discovery・購読（接続後／security完了後に呼ぶ）
- `midi.onMidiMessage(callback)` — デコード済み`EspBleMidiMessage`（status/data1/data2/timestamp）
- `midi.sendNoteOn/sendNoteOff/sendControlChange/sendProgramChange(connectionId, ...)`
- `midi.sendSysEx(connectionId, data, length)` — framed SysExを送信。大きなメッセージは自動で複数パケットへ分割

## 期待されるSerial出力

```
MIDI: conn=1 status=0x90 data1=60 data2=100 ts=165
```
