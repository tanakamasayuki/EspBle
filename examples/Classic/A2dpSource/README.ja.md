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
