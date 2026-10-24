# MidiHost

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 7章「BLE MIDI編 — 楽器としてつながる」

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

## 関連するガイド

- [BLE入門ガイド §7 BLE MIDI編](../../../docs/GUIDE_BLE_BASICS.ja.md#7-ble-midi編--楽器としてつながる) — packet形式とtimestamp

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[MidiHost.ino](MidiHost.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

MidiHost - CentralとしてBLE MIDI Peripheralへ接続する。MIDI Serviceをscan → 接続 → Discovery → 購読 → MIDIメッセージ表示。Serial入力でノート送信。 MidiDevice example や市販BLE MIDI楽器と接続できる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
4. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
5. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

BLE MIDIは演奏イベントの通信で、録音音声を送るものではありません。受信アプリでMIDI入出力を選び、Note Onに対応するNote Offを送り、チャンネル・音程・強さをAPIの形式で指定します。この例はOn直後にOffを送るため、音を持続させたい場合はloopを止めない時刻管理を追加します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
