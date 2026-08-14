# HidVendorHost（Classic）

> English: [README.md](README.md)

標準では説明されないreportを送るClassic HID deviceのHost側です。address指定で接続し、
input reportをbyte列として表示します。vendor固有deviceに必要なのはこれで、Host側の
復号はkeyboardとmouseしか扱わず、それ以外はrawで届きます。**Classicは無印ESP32のみ**で
動き、接続先はClassic HID deviceです。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID device 1台: [HidVendorDevice](../HidVendorDevice/)を動かす無印ESP32、
  または任意のBR/EDR HID機器

## 何をするか

- serial consoleへ入力したaddressへ接続する
- input reportをreport IDとbyte列で表示する
- 受理された後に失敗した接続を通知する

## 主なAPI

- `bluetooth.hidHost().connect(address)` — Classicには絞り込むadvertisementが無いため
  addressが必要
- `onInputReport()` — deviceが送ったままのreport
- `onConnectionFailed()` — 後から非同期に失敗したことを受け取る

## 補足

**Classic HID Hostが復号するのはkeyboardとmouseだけです。**consumer / system / gamepad /
vendorのreportは`onInputReport()`へrawで届くため、この例はbyte列を表示します。BLE側の
hostはより多くを復号します——[BLEとClassic](../../../docs/CLASSIC_VS_BLE.ja.md)を参照して
ください。

report IDを宣言するdeviceのreportは、payload先頭にIDが入ります。そのため`value`と
`reportId`は同じことを2度示します。

HID Hostの接続は同時に1つです。
