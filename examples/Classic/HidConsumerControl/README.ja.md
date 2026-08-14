# HidConsumerControl（Classic）

> English: [README.md](README.md)

Bluetooth Classic（BR/EDR）のメディアキーとシステム要求です。car audioや古いTVは
Classic HIDを受け付けるため、その相手にはこちらを使います。BLE側の
[Hid/ConsumerControl](../../Hid/ConsumerControl/)と対になります。
**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- メディアキーに反応するClassic HID host 1台: car audio、TV、PCなど

## 動作

- Consumer ControlとSystem Controlを設定する。Hostが電源・スリープをメディアキーとは
  別扱いにするため、profileも別になっている
- Serialコマンドで音量、再生/一時停止、次の曲、スリープ要求を送る

## 主なAPI

- `bluetooth.hidConsumerControl().click(usage)` — 押下と解放を送る。Hostは押下で
  動作し、解放が無いと繰り返しが止まらない
- `bluetooth.hidSystemControl().click(usage)` — 電源、スリープ、復帰の要求

## Serialコマンド

| キー | 動作 |
|---|---|
| `+` / `-` | 音量を上げる / 下げる |
| `p` | 再生・一時停止 |
| `n` | 次の曲 |
| `s` | スリープ要求（Generic Desktopのusage 0x82） |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い
