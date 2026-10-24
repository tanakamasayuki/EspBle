# HfpAudioGatewayRaw（Classic）

> English: [README.md](README.md)

HFPの電話機側です。headsetがこの基板へ接続してボタンを押し、このsketchが電話機のように
応答します。電話機なしでheadsetを試せるのがこの構成の利点です。**Classicは無印ESP32のみ**
で動き、届く相手はClassicのHFP headsetやhands-free機器です。**ClientとAudio Gatewayの
roleは同一process内で排他**なので、この基板は同時に[HfpClientRaw](../HfpClientRaw/)には
なれません。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- HFP headset 1台、または[HfpClientRaw](../HfpClientRaw/)を動かす無印ESP32

## 何をするか

- operator名とsubscriber番号を持ってservice-level connectionへ応答する
- 発信・応答・終話を処理し、headsetが見る通話状態を動かす
- SCO frameごとにcodec、長さ、bad frame flagを表示する

## 主なAPI

- `EspBleClassicHfpAudioGatewayConfig` — `operatorName`、`subscriberNumber`、
  `preferredAudioCodec`。前2つが`AT+COPS`と`AT+CNUM`の応答になる
- `onCommand()` — headsetが要求した内容。`Dial`、`Answer`、`Hangup`、`Dtmf`、
  `VoiceRecognition`、`NoiseReduction`、`DialMemory`、`UnknownAt`
- `respondToCommand(accepted)` — 受理または拒否。無応答はheadsetを待たせ続ける
- `reportOutgoingCall()` / `reportCallActive()` / `reportCallEnded()` /
  `reportIncomingCall()` — headsetが表示する通話状態
- `respondToUnknownAt(text)` — backendが解釈しないAT（Apple拡張など）へ応答する。
  交換を終わらせるOKはlibraryが送る

## 補足

`DialMemory`が運ぶのは電話機内の位置で、番号ではありません。位置を桁として掛けると別の
相手に繋がるため、専用のcommand typeで届きます。

**`onAudio()`から戻る前にSCO payloadをcopyしてください。**CVSD／mSBCのdecode、buffering、
device I/OはEspBleの外側です。実機ではmSBCの57 byte送信が58／60 byteのpadding付きで届き、
bad frameも60 byteで届きます。長さとflagを渡せばdecoderが欠損を補えます。

このAudio Gatewayは意図的に単一call modelです。通話待ちと三者通話は未実装です。

## 関連するガイド

- [Classic入門ガイド §8 HFP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#8-hfp) — SLC、call control、raw SCO

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HfpAudioGatewayRaw.ino](HfpAudioGatewayRaw.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `gateway.onAudio(...)` — この符号化SCO音声コールバックはBluetooth Hostの実行コンテキストで動きます。戻る前にデータをコピーし、処理を短く保ち、内部からコールバックの再登録やendを呼ばないようにします。

## 確認と変更のポイント

この例の通話状態コマンドはAudio Gatewayの模擬動作で、実際の電話回線ではありません。Service Level接続とSCO音声接続は別です。実用時は通話の接続先と外部CVSD／mSBC処理を用意します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。上記のスタック／Host側コールバックは例外なので、updateから呼ばれると考えず、その場で必要なコピーや応答だけを短く処理します。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
