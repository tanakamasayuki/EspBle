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
