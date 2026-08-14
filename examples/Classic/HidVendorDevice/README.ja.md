# HidVendorDevice（Classic）

> English: [README.md](README.md)

Report Descriptorをsketch側で書くClassic HID deviceです。usage pageはvendor用のものを
使います。keyboard、mouse、gamepad、メディアキーのどれでもない機器のための逃げ道で、
report構造はdescriptorが決め、Hostはそれに従います。**Classicは無印ESP32のみ**で動き、
届く相手はClassic HID Host——旧世代のゲーム機やPCです。BLEを話すHost向けの同等品は
[Hid/CustomDevice](../../Hid/CustomDevice/)です。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID host 1台: PC、または[HidVendorHost](../HidVendorHost/)を動かす無印ESP32

## 何をするか

- vendor usage pageで4 byteのinputをreport ID 1として宣言する
- 定期的にreportを送り、Host側で見えるようにする
- Hostの接続・切断を表示する

## 主なAPI

- `EspBleClassicHidDeviceConfig::reportDescriptor` / `reportDescriptorLength`
  — HostがSDPで読むdescriptor
- `bluetooth.hidDevice().begin(hidConfig)` — device recordを登録する
- `sendInputReport(reportId, data, length)` — descriptorに合ったreportを送る

## 補足

descriptorとdevice名などの文字列は1つのSDP recordを共有し、合計214 byteまでです。
超えるdescriptorは`begin()`が`ResourceExhausted`で拒否します——Hostから見えないdeviceを
起動しないためです。

汎用のHostはvendor usageを解釈しません。相手側も合わせて書く必要があり、
[HidVendorHost](../HidVendorHost/)はrawのreportを表示することでそれを示します。
