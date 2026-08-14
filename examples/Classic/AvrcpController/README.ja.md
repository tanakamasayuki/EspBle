# AvrcpController（Classic）

> English: [README.md](README.md)

AVRCP Controllerです。相手側の再生・停止を操作する側になります。Target側（押された
操作を受ける側）は[A2dpSinkAvrcp](../A2dpSinkAvrcp/)にあります。AVRCPは操作だけを運び、
音声はA2DPが運びます。AVRCPの接続はA2DP接続に追従します。
**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- 再生側 1台: 携帯、または[A2dpSinkAvrcp](../A2dpSinkAvrcp/)を動かす無印ESP32

`playerAddress`に相手のaddressを入れます。[Inquiry](../Inquiry/)で探せます。

## 動作

- backendの要件によりAVRCPをA2DPより先に開始する
- Controller roleだけを動かす。両方を動かすこともでき、A2dpSinkAvrcpがその例
- 再生操作キーを送り、相手が受理したかを表示する。`accepted=0`はcommandを理解した上で
  拒否したという意味で、届かなかった場合とは別である
- play statusとmetadataを要求し、absolute volumeとplayer settingを設定する

## 主なAPI

- `bluetooth.avrcp().sendKey(command)` / `onPassthroughResponse()`
- `requestPlayStatus()` / `onPlayStatus()`
- `requestMetadata(mask)` / `onMetadata()`
- `setAbsoluteVolume(0〜127)` — パーセントではない
- `setPlayerSetting(attribute, value)` — repeatやshuffleなど

## Serialコマンド

| キー | 動作 |
|---|---|
| `c` | mediaを接続し、AVRCPもそれに追従する |
| `p` / `x` | 再生 / 一時停止 |
| `n` / `b` | 次 / 前の曲 |
| `i` | play statusを要求 |
| `m` | title・artist・albumを要求 |
| `v` | absolute volumeを64にする |
| `r` | 1曲リピートにする |
