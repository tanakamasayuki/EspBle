# MidiDevice

> English: [README.md](README.md)

標準のBLE MIDI Serviceを使ってBLE MIDI PeripheralをAdvertiseします。Serial入力でNote On/Offを送信し、接続したHostから届くMIDIを表示します。[MidiHost](../MidiHost/) exampleや一般的なBLE MIDI Host（スマホ/タブレットのDAW等）と接続できます。

## 必要なもの

- 1 × ESP32-S3（このsketch。MIDI Device / Peripheral）
- 1 × BLE MIDI Host: スマホ/タブレットのDAW、PC、または[MidiHost](../MidiHost/) exampleを動かす2台目

## 動作

- `begin()`の前にBLE MIDI ServiceとI/O characteristicを登録します（Service UUIDはAdvertisingへ追加）
- Serialコマンド`n`で中央ハのNote On → Note Offを送信します（Hostが購読中のときのみ）
- Hostから届いたMIDI（Host → Device）をSysExチャンクも含めて表示します

## 主なAPI

- `EspBleMidiDevice midi(ble)` — `EspBle`インスタンスへの参照で構築
- `midi.begin()` — Serviceを登録。`ble.begin()`より前に呼ぶ
- `midi.noteOn(channel, note, velocity)` / `midi.noteOff(...)` — channel voiceメッセージを送信
- `midi.onMessage(callback)` — Hostから届いたMIDIを`EspBleMidiMessage`へデコード
- `midi.ready()` — Hostが購読中はtrue

## 期待されるSerial出力

```
MIDI in: status=0xb0 data1=7 data2=100 ts=1234
SysEx chunk: start=1 end=0 length=16
```
