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
