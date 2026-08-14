# HidComposite（Classic）

> English: [README.md](README.md)

keyboard、mouse、メディアキーを兼ねる1台のBluetooth Classic（BR/EDR）HID deviceです。
Classicはdevice recordを1つ登録するため、すべてが1つの合成Report Descriptorに入り、
profileごとにreport IDが分かれます。

**合成できるprofile数にはBLEには無い上限があります。**Report Descriptorとprofileの
文字列3つは1つのSDP recordを共有し、合計214 byteまでです。この3つでdescriptorは
144 byte、文字列が57 byteなので201 byteで収まります。gamepadを加えるとdescriptorが
212 byteになり、何も登録されません。`begin()`はそうした組み合わせを拒否します——誰も到達できない
deviceを起動しないためです。gamepadは[HidGamepad](../HidGamepad/)を単独で使ってください。BLE側の
[Hid/CompositeKeyboardMouse](../../Hid/CompositeKeyboardMouse/)と対になります。
**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす無印ESP32

## 動作

- `begin()`前に3つのprofileを設定する。設定した内容がdescriptorを決めるため、後から
  追加してもHostが読み終えたrecordには入らない
- Class of Deviceは1つ選ぶ。複合deviceでも、Hostが提示する姿は1つに決まる
- Serialコマンドでprofileごとに1 reportを送る

## 主なAPI

- `hidKeyboard()` / `hidMouse()` / `hidConsumerControl()` — まとめて設定し、個別に送る
- 送信はそれぞれのreport IDで出るため、keyboardしか解釈しないHostは他を無視する

## Serialコマンド

| キー | 動作 |
|---|---|
| `k` | "hi"と入力 |
| `m` | pointerを移動 |
| `v` | 音量を上げる |
| `r` | keyboardとmouseを解放 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い
