# A2dpSource（Classic）

> English: [README.md](README.md)

A2DP Sourceです。この機器がspeakerやheadsetへ音声を送ります。EspBleはencode済みの
SBC frameを運ぶだけでencodeはしないため、ここでは固定表のframeを送ります。実際の
sketchではPCMFlowBluetooth等のencoderから受け取ります。受け取る側は
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
