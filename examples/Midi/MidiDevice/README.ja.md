# MidiDevice

> English: [README.md](README.md)

ボードをBLE MIDI Peripheralにします。スマホ/タブレットのDAWや[MidiHost](../MidiHost/) exampleからペアリングし、Serialでノートを送信、Hostから届くMIDIを表示します。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。MIDI Device / Peripheral）
- 1 × BLE MIDI Host: スマホ/タブレットのDAW、PC、または[MidiHost](../MidiHost/) exampleを動かす2台目

## 動作

- `begin()`の前にBLE MIDI ServiceとI/O characteristicを登録します（Service UUIDはAdvertisingへ追加）
- `n`で中央ハのNote On → Note Offを送信します
- Hostから届いたMIDI（Host → Device）を表示します

## Serialコマンド

| コマンド | 動作 |
|---------|------|
| `n` | Note On（中央ハ）→ Note Off送信 |

## 主なAPI

- `EspBleMidiDevice midi(ble)` — 参照で構築（`EspUsbDeviceMidi(device)`と同様）
- `midi.begin()` — Serviceを登録。`ble.begin()`より前に呼ぶ
- `midi.noteOn/noteOff/controlChange/programChange/polyPressure/channelPressure/pitchBend(...)`
- `midi.sendSysEx(data, length)` — framed SysEx（`0xF0 .. 0xF7`）を送信。大きなメッセージは自動で複数パケットへ分割
- `midi.onMessage(callback)` — Hostから届いたMIDIを`EspBleMidiMessage`へデコード
- `midi.ready()` — Hostが購読中はtrue

## 期待されるSerial出力

```
MIDI in: status=0xb0 data1=7 data2=100 ts=1234
```
