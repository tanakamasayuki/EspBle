# SppServer（Classic）

> English: [README.md](README.md)

SPPの待ち受け側です。Classic上のbinary-safeなbyte streamで、PCやAndroidからは
serial portとして見えます。**Classicは無印ESP32のみ**で動き、届く相手はClassic機器
——PC、Android、他のESP32です。**iOSのアプリからSPPは使えません**（MFi機器を除く）。
iPhone相手なら独自GATT serviceのBLEにします。`Serial`向けに書かれたcodeをそのまま
使いたい場合は[SppStream](../SppStream/)を参照してください。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- SPP client 1台: serial terminalを持つPCやAndroid、または[SppClient](../SppClient/)を
  動かす無印ESP32

## 何をするか

- SPP serviceを1つ公開して待つ
- 受け取ったbyteをそのまま返す
- 開いているsessionを覚え、書き込み先を把握する

## 主なAPI

- `bluetooth.spp().startServer()` — serviceを公開する。繰り返し呼べば最大4つ公開でき、
  それぞれ別のRFCOMM channelを得る
- `onConnected()` / `onDisconnected()` — session id。読み書きはすべてこのidを指定する
- `onData()` — 受信したbyte。`available()` / `read()`は同じbufferをstreamとして読む形
- `write(sessionId, value)` — queueへ入るだけで、送信完了は`onWriteCompleted()`

## 補足

SPPはbinary-safeです。payload途中の`0x00`は終端ではなくデータです。

SPP自体はpairingを必須としませんが、相手が要求することがあります。sketchから制御する
場合は[SppPairing](../SppPairing/)を参照してください。

## 関連するガイド

- [Classic入門ガイド §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#4-spp) — RFCOMM channel、複数service、byte stream
- [EspBleを深く使う](../../../docs/GUIDE_ADVANCED.ja.md) — 990 byteのpacket、8件の送信queue、backpressure
