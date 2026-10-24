# HfpClientRaw（Classic）

> English: [README.md](README.md)

HFPのheadset側です。通話の制御経路、encode済みのSCO音声、そして機器が電話機へ伝える情報を
扱います。**EspBleが運ぶのはencode済みpayload**で、CVSDやmSBCのdecodeとspeakerの駆動は別
libraryの担当です。**Classicは無印ESP32のみ**で動き、届く相手はClassicの電話機やAudio
Gatewayです。**ClientとAudio Gatewayのroleは同一process内で排他**なので、この基板は同時に
[HfpAudioGatewayRaw](../HfpAudioGatewayRaw/)にはなれません。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- 電話機 1台、または[HfpAudioGatewayRaw](../HfpAudioGatewayRaw/)を動かす無印ESP32

Audio Gatewayのaddressを`audioGatewayAddress`へ入れます。[Inquiry](../Inquiry/)で探せます。

## 何をするか

- 接続し、接続状態と通話状態を表示する
- service-level connectionが確立したら機器の情報を伝える: Apple拡張を有効にし、電池残量を
  報告し、電話機側のnoise reductionを止めるよう頼む
- network operatorとsubscriber番号を問い合わせ、答えを表示する
- SCO frameをencode済みviewとして受け取る

## 主なAPI

- `bluetooth.hfpClient().connect(address)` / `onConnectionChanged()` /
  `serviceLevelConnected()`
- `enableAppleExtensions(identification)`の後に`reportBatteryLevel(level, docked)`
  — levelは0〜9。先に拡張を有効にする必要がある
- `disableNoiseReduction()` — 自前でDSPを持つ機器のため
- `queryOperatorName()` / `requestSubscriberNumber()` — 要求。答えは
  `onOperatorName()` / `onSubscriberNumber()`へ届く
- `dialMemory(location)` — 電話機内の位置を指定して発信する
- `onAudio()` — encode済みSCO。`audio.data`はcallback内でのみ有効

## 補足

**機器情報の送信はすべてservice-level connectionが必要**で、ACL linkだけでは足りません。
そのためこのsketchは`connect()`の戻りではなく`serviceLevelConnected()`を待ちます。

電話機が空文字で答えることもあり、`disableNoiseReduction()`を無視することもあります。
noise reductionを戻す呼び出しはありません——要求はその接続の間だけ有効です。

**`onAudio()`から戻る前にSCO payloadをcopyしてください。**`badFrame`を保てば、mSBC decoderが
クリックノイズにせず欠損を補えます。

ClientもAudio Gatewayと同じく意図的に単一call modelです。通話待ち・三者通話（CHLD、BTRH）は
未実装です。

## 関連するガイド

- [Classic入門ガイド §8 HFP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#8-hfp) — SLC、call control、raw SCO

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HfpClientRaw.ino](HfpClientRaw.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

HFPのheadset側。通話の制御経路と、encode済みのSCO音声を扱う。EspBleが運ぶのは encode済みpayloadなので、CVSDやmSBCのdecodeとspeakerの駆動は別libraryの担当。 機器側から伝えることもあり、それがこのsketchの後半である。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `bluetooth.hfpClient().onAudio(...)` — この符号化SCO音声コールバックはBluetooth Hostの実行コンテキストで動きます。戻る前にデータをコピーし、処理を短く保ち、内部からコールバックの再登録やendを呼ばないようにします。
3. `bluetooth.hfpClient().connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

実際のAudio Gatewayへ接続し、Service Levelと音声接続のイベントを別々に確認します。符号化SCOフレームの受信はマイク／スピーカー入出力とは異なり、コーデックとバッファ処理はHostコールバックの外で行います。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。上記のスタック／Host側コールバックは例外なので、updateから呼ばれると考えず、その場で必要なコピーや応答だけを短く処理します。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
