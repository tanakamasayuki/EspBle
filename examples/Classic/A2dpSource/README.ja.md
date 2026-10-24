# A2dpSource（Classic）

> English: [README.md](README.md)

A2DP Sourceです。この機器がspeakerやheadsetへ音声を送ります。EspBleはencode済みの
SBC frameを運ぶだけでencodeはしないため、ここでは固定表のframeを送ります。実際の
sketchでは、正式リリース済みの
[PCMFlowBluetooth](https://github.com/tanakamasayuki/PCMFlowBluetooth)の`SbcEncoder`を使用します。
M5Stack Core2のmicrophoneから送信する完全な例は同libraryの`A2dpSourceM5Microphone`にあります。受け取る側は
[A2dpSinkRaw](../A2dpSinkRaw/)です。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- A2DP Sink 1台: Bluetooth speaker、または[A2dpSinkRaw](../A2dpSinkRaw/)を動かす無印ESP32

`speakerAddress`にspeakerのaddressを入れます。[Inquiry](../Inquiry/)で探せます。

## 動作

- negotiationの結果を表示する。SinkはこのSourceが提示した候補から選ぶため、encoderは
  この結果に合わせる（逆ではない）
- streaming中はloopごとに1 frameを送る
- `WouldBlock`をerrorではなくbackpressureとして扱う。frameは捨てずに保持して再送する。
  捨てるとstreamに欠落が出る
- Sinkが報告する自身の再生遅延を表示する。映像を出すSourceはこの分だけ絵を遅らせる

## 主なAPI

- `bluetooth.a2dpSource().begin()` / `connect(address)` / `start()` / `suspend()`
- `send(packet)` — `Accepted`、`WouldBlock`、または失敗を返す
- `onSinkDelay()` — Sink側の再生遅延（1/10 ms単位）

## Serialコマンド

| キー | 動作 |
|---|---|
| `c` | speakerへ接続 |
| `s` | streaming開始 |
| `p` | 一時停止 |
| `d` | 切断 |

## 関連するガイド

- [Classic入門ガイド §7 A2DPとAVRCP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#7-a2dpとavrcp) — encode済みmedia、codec設定、操作
- [PCMFlowBluetoothのA2DP Source example](https://github.com/tanakamasayuki/PCMFlowBluetooth/tree/main/examples/A2dpSourceM5Microphone) — microphone、SBC encode、backpressure-safe送信

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[A2dpSource.ino](A2dpSource.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[A2dpSinkRaw](../A2dpSinkRaw/README.ja.md)です。2台で試す場合は相手に対応するClassicプロファイルを用意し、必要に応じて接続を受ける側を発見可能にして、開始側へそのアドレスを設定します。

## この例の構成

A2DP Source。この機器がspeakerやheadsetへ音声を送る側になる。EspBleは encode済みのSBC frameを運ぶだけでencodeはしないため、ここでは固定表の frameを送る。実際のsketchではPCMFlowBluetoothのSbcEncoderから受け取る。 受け取る側はexamples/Classic/A2dpSinkRaw。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `bluetooth.a2dpSource().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `bluetooth.a2dpSource().onCodecConfigured(...)` — 交渉で決まったコーデック設定を使います。符号化パケットはPCMサンプルではないので、対応する外部コーデック処理が必要です。
4. `bluetooth.a2dpSource().onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
5. `bluetooth.a2dpSource().connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

この例は表にある符号化済みフレームを送ります。実際のエンコーダは交渉後の設定に従います。sendがWouldBlockを返したらフレームを保持して再試行し、黙って捨てて音が欠けないようにします。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
