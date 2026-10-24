# A2dpSinkRaw（Classic）

> English: [README.md](README.md)

A2DPの受信側です。電話機やPCからこの基板へ音楽が流れてきます。**EspBleが運ぶのは
encode済みのpayload**です。SBCのdecodeとspeaker出力まで動かす場合は、正式リリース済みの
[PCMFlowBluetooth](https://github.com/tanakamasayuki/PCMFlowBluetooth)にある
`A2dpSinkM5Speaker`または`A2dpSinkToPcm`から始めてください。**Classicは無印ESP32のみ**で動き、届く相手はClassicのsource——電話機、
tablet、PCです。BLEにはこのlibraryが扱う標準audio pathが無いため、BLE版はありません。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- A2DP source 1台: 電話機・tablet・PC、または[A2dpSource](../A2dpSource/)を動かす
  無印ESP32

## 何をするか

- sourceからの接続を受け、確定したmedia MTUを表示する
- sourceが選んだSBC設定（sample rate、channel数、bitpool）を表示する
- encode済みpacketとbyteを数え、定期的に表示する

## 主なAPI

- `bluetooth.a2dpSink().begin()` — Sinkを開始する。操作も扱うなら
  [A2dpSinkAvrcp](../A2dpSinkAvrcp/)のように`bluetooth.avrcp().begin()`を先に呼ぶ
- `onCodecConfigured()` — sourceが選んだ設定。decoderに必要な情報
- `onMedia()` — encode済みframe。`audio.data`はcallback内でのみ有効
- `setDelay(tenthsOfMillisecond)` — 再生までにかかる時間をsourceへ伝える

## 補足

**`onMedia()`から戻る前にpayloadをcopyしてください。**viewはbackendのbufferを指すため、
別の場所でdecodeするなら上限のあるqueueへcopyを積みます。この例のようにbyteを数えるだけ
ならcopyは不要です。

自分の再生遅延を知っているのはSink側だけです。`setDelay()`へ渡す値はapplicationが計測する
ものであり、libraryには分かりません。

## 関連するガイド

- [Classic入門ガイド §7 A2DPとAVRCP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#7-a2dpとavrcp) — encode済みmedia、codec設定、操作
- [PCMFlowBluetoothのA2DP Sink example](https://github.com/tanakamasayuki/PCMFlowBluetooth/tree/main/examples) — SBC decode、PCMFlow、speaker出力

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[A2dpSinkRaw.ino](A2dpSinkRaw.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[A2dpSource](../A2dpSource/README.ja.md)です。2台で試す場合は相手に対応するClassicプロファイルを用意し、必要に応じて接続を受ける側を発見可能にして、開始側へそのアドレスを設定します。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.a2dpSink().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
2. `bluetooth.a2dpSink().onCodecConfigured(...)` — 交渉で決まったコーデック設定を使います。符号化パケットはPCMサンプルではないので、対応する外部コーデック処理が必要です。
3. `bluetooth.a2dpSink().onMedia(...)` — この符号化音声コールバックはBluetooth Hostの実行コンテキストで動きます。参照中のデータを容量制限付きキューへコピーして戻り、デコードや再生は別の処理で行います。
4. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。

## 確認と変更のポイント

受信するのはPCMではなく符号化SBCで、この例は情報を表示します。音を出すにはデコーダ、容量制限付きバッファ、音声出力経路が必要で、Hostコールバックの外で処理します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。上記のスタック／Host側コールバックは例外なので、updateから呼ばれると考えず、その場で必要なコピーや応答だけを短く処理します。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
