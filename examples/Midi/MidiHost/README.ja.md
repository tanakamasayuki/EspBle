# MidiHost

> English: [README.md](README.md)

CentralとしてBLE MIDI Peripheralへ接続します。BLE MIDI Serviceをscan → 接続 → Discovery → 購読 → デコード済みMIDIを表示。Serial入力でノートを送信します。[MidiDevice](../MidiDevice/) exampleや市販BLE MIDI楽器と接続できます。

## 必要なもの

- 1 × ESP32-S3（このsketch。MIDI Host / Central）
- 1 × BLE MIDI Peripheral: [MidiDevice](../MidiDevice/) exampleまたは市販BLE MIDI楽器

## 動作

- BLE MIDI Serviceをscanし、最初のconnectableな相手へ接続します
- 接続直後にMIDI I/O characteristicをDiscovery・購読します（この例はsecurity無効）
- デコード済みMIDIをSysExチャンクも含めて表示します
- Serialコマンド`n`で中央ハのNote On → Note Offを送信します（接続・購読後のみ）

## 主なAPI

- `EspBleMidiHost midi(ble)` — `EspBle`インスタンスへの参照で構築
- `midi.begin()` — HostのGATTコールバックを設定。`ble.begin()`の後に呼ぶ
- `midi.discover(connectionId)` — Discovery・購読（接続後／security完了後に呼ぶ）
- `midi.onMidiMessage(callback)` — デコード済み`EspBleMidiMessage`（status / data1 / data2 / timestamp）
- `midi.sendNoteOn(connectionId, ...)` / `midi.sendNoteOff(connectionId, ...)` — Deviceへ送信
- `midi.ready(connectionId)` — 購読完了後はtrue

## 期待されるSerial出力

```
MIDI: conn=1 status=0x90 data1=60 data2=100 ts=165
```
