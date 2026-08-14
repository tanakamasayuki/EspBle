# HidKeyboardNkro（Classic）

> English: [README.md](README.md)

N-key rollover対応のBluetooth Classic（BR/EDR）keyboardです。各キーがreport内の
1 bitなので6キー制限がありません。BLE側の[Hid/KeyboardNkro](../../Hid/KeyboardNkro/)と
対になります。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす無印ESP32

## 動作

- `enableNkro(true)`を`configure()`より**前に**呼ぶ。NKROはReport Descriptorを変え、
  Hostはpairing時にそれを一度だけ読むため
- 8キー同時押しを送る（6キーのreportでは表現できない）
- 文字入力は6KROと同じ便利APIで行う

## 主なAPI

- `bluetooth.hidKeyboard().enableNkro(true)` — `configure()`より前に呼ぶ
- `pressUsage(usage)` — 押下状態へ追加し、状態全体が1 reportで送られる
- `write("...")` — descriptorの選択によらず同じ

## Serialコマンド

| キー | 動作 |
|---|---|
| `8` | 8キー同時押し |
| `w` | "nkro"と入力 |
| `r` | すべて解放 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い
