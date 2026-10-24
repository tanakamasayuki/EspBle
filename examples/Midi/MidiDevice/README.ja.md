# MidiDevice

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 7章「BLE MIDI編 — 楽器としてつながる」

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

## 関連するガイド

- [BLE入門ガイド §7 BLE MIDI編](../../../docs/GUIDE_BLE_BASICS.ja.md#7-ble-midi編--楽器としてつながる) — packet形式とtimestamp

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[MidiDevice.ino](MidiDevice.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

MidiDevice - BLE MIDI PeripheralをAdvertiseする。Serial入力でNote On/Offを送信し、 接続したHostから届くMIDIを表示する。MidiHost example や一般的なBLE MIDI Host （スマホ/タブレットのDAW等）と接続できる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `midi.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。

## 確認と変更のポイント

BLE MIDIは演奏イベントの通信で、録音音声を送るものではありません。受信アプリでMIDI入出力を選び、Note Onに対応するNote Offを送り、チャンネル・音程・強さをAPIの形式で指定します。この例はOn直後にOffを送るため、音を持続させたい場合はloopを止めない時刻管理を追加します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
