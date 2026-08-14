# HidKeyboardHost（Classic）

> English: [README.md](README.md)

復号済みのkeyboard / mouse eventを受け取るBluetooth Classic（BR/EDR）HID hostです。
callbackとevent型はBLE host（[Hid/KeyboardHost](../../Hid/KeyboardHost/)）と同じで、
違うのは無線と相手の指定方法だけです。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台（HID host）
- Classic HID device 1台: keyboard、または[HidKeyboard](../HidKeyboard/)や
  [HidGamepad](../HidGamepad/)を動かす無印ESP32

`keyboardAddress`に相手のaddressを入れます。[Inquiry](../Inquiry/)で探せます。

## 動作

- addressを指定して接続する。Classicには絞り込むadvertisementが無いため
- SDPで受け取ったReport Descriptorからkeyboard / mouse reportを復号する。独自の
  layoutを持つdeviceでもeventとして届く
- 同じreportのusage単位eventより先にkeyboard stateを配送する（BLE hostと同順）
- 分類できないreportは`onInputReport()`へraw（payloadの前にreport ID）で渡す
- LEDのreport IDは仮定せず相手のdescriptorから取る

## 主なAPI

- `bluetooth.hidHost().connect(address)`
- `onKeyboardState()` / `onKeyboard()` / `onMouse()` / `onInputReport()`
- `setKeyboardLayout()` — usageをどの文字と解釈するかはこちら側のlayoutが決める。
  usageの選択はdeviceが自分のlayoutで行っている
- `setKeyboardLeds(numLock, capsLock, scrollLock)`

## Serialコマンド

| キー | 動作 |
|---|---|
| `c` | Caps Lock LED点灯 |
| `0` | 全LED消灯 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い
