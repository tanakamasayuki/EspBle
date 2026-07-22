# VendorDevice

> English: [README.md](README.md)

任意のInput / Output / Featureデータ用に、vendor定義のReport（固定Report ID 6）を公開するBLE HID device（HID over GATT / HOGP）です。HID Service `0x1812` をadvertiseし、InputはSerialコマンドで送信、Hostからの書込みは表示します。Report sizeは1〜64 bytesで構成でき、ここでは8を使います。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: [VendorHost](../VendorHost/)を動かす2台目のボード

## 動作

- `begin()` 前に `EspBleHidVendorConfig::reportSize = 8` でvendor HID profileを構成します
- preferred MTUに100を要求します
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- Hostが書き込んだOutput / Feature Reportを `onOutputReport()` / `onFeatureReport()` で表示します
- 切断のたびにadvertisingを再開します
- `i` で8byteのVendor Input Report（`'E' 'S' 'P'` ＋ 回転カウンタ ＋ `4 5 6 7`）を送信

## 主なAPI

- `ble.hidVendor().configure(vendorConfig)` — `EspBleHidVendorConfig` で `reportSize` を設定。失敗時はfalseを返す
- `vendor.sendInput(data, length)` — Vendor Input Reportを送信（成否を返す）
- `vendor.onOutputReport(cb)` / `vendor.onFeatureReport(cb)` — Hostからの書込みを `EspBleHidVendorReport`（`reportType`、`length`、`data`）で受信

## メモ

- Reportレイアウトと Report ID 6 はEspUsbDeviceのVendor HIDに揃えています。
- Output / Featureイベントは `ble.update()` から配送されます。

## 期待されるSerial出力

```
Send 'i' to send an 8-byte Vendor Input Report.
Input: sent
Output type=2 length=8 data=4f 55 54 03 04 05 06 07
```
