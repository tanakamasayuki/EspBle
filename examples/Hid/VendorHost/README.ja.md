# VendorHost

> English: [README.md](README.md)

vendor定義のHIDデバイス向けBLE HID Host（Central）です。HID Service `0x1812` をscanし、Pairing・Discovery後にVendor Input Reportを受信し、Vendor Output / Feature Reportを書き込みます。security完了後の `discover()` フローはほかのHID種別とすべて同じです。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Host / Central）
- HIDデバイス × 1: [VendorDevice](../VendorDevice/)を動かす2台目のボード

## 動作

- HID Service `1812` をadvertiseする最初の機器をscanして接続します
- preferred MTUに100を要求し、Bondingつきでsecurityを有効化します
- 暗号化成功後に `onSecurityChanged` からHID Discoveryを開始します
- 受信したVendor Input Reportを `onVendorInput()` で表示します（Report ID、length、raw bytes）
- `o` で8byteのVendor Output Report、`f` で8byteのVendor Feature Reportを書き込み（接続中のみ）

## 主なAPI

- `ble.hidHost().discover(connectionId)` — security完了後にHID Discoveryを開始
- `hidHost().onDiscovered(cb)` — `success` / `detail` を持つ `EspBleHidKeyboardHostDiscovery`
- `hidHost().onVendorInput(cb)` — `reportId`、`rawLength`、`rawData` を持つ `EspBleHidVendorInputEvent`
- `hidHost().sendVendorOutput(connectionId, data, length)` / `sendVendorFeature(connectionId, data, length)` — デバイスへ書込み（成否を返す）

## 期待されるSerial出力

```
Send 'o' for Output or 'f' for Feature after discovery.
HID discovery: ready
Vendor Input report=6 length=8 data=45 53 50 00 04 05 06 07
Output: sent
```
