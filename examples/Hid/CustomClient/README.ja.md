# CustomClient

> English: [README.md](README.md)

**汎用GATTクライアント**でCustom HIDデバイスの任意Report Descriptorを読み、入力Reportを受け取ります。[Hid/CustomDevice](../CustomDevice/) とペアです。

HIDデバイスは同一UUID `0x2A4D` のReport characteristicを複数公開します。汎用GATTクライアントはUUIDでcharacteristicを指定するため、この例は最初のReport characteristic（CustomDeviceが最初に登録する入力Report）を購読します。（個々のReportをhandle単位で細かく扱うには `ble.hidHost()` を使ってください。）

## ハードウェア

- ESP32-S3 × 1（このスケッチ。central / GATTクライアント）
- ESP32-S3 × 1（Hid/CustomDevice。HIDデバイス / peripheral）

## 動作

- HID Service（`0x1812`）をadvertiseするデバイスを探して接続
- Report Map（`0x2A4B`）を読み、byte数を表示
- 入力Report（`0x2A4D`）を購読し、2byteのReport（ダイヤル差分＋ボタン）をデコード

## 補足

- CustomDeviceの例はsecurity有効で動作するため、bondingしないクライアントは拒否される場合があります。暗号化なしで試すにはデバイス側のsecurityを無効化する（またはこのクライアントにbondingを追加する）ようにしてください。
