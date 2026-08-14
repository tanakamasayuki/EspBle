# HidKeyboard（Classic）

> English: [README.md](README.md)
> どちらの無線を使うか: [BLEとClassic](../../../docs/CLASSIC_VS_BLE.ja.md)

Bluetooth Classic（BR/EDR）のkeyboardとmouseです。reportとdescriptorを共有している
ため、呼び出しはBLE版の[Hid/KeyboardDevice](../../Hid/KeyboardDevice/)と同じで、
違うのは無線だけです。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台（HID device）
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす無印ESP32

## 動作

- keyboardとmouseのprofileを`begin()`より前に設定する。合成したReport Descriptorは、
  Hostがpairing時に読むdevice recordの一部になるため
- Hostが返すLED状態を表示する
- Serialコマンドで文字入力、単キー、pointer移動を行う

## 主なAPI

- `bluetooth.hidKeyboard().configure()` / `bluetooth.hidMouse().configure()`
- `write("...")` / `tapKey(char)` / `pressUsage(usage)` / `releaseAll()`
- `onOutputReport()` — HostのCaps Lock・Num Lock状態
