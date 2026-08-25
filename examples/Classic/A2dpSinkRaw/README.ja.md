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
