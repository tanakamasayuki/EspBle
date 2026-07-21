# BondManagementClient

> English: [README.md](README.md)

Bond Management Service（0x181E）へ接続し、Bond Management Feature bit fieldをRead、Bond Management Control Pointへ「Delete bond of requesting device（LE）」（0x03）を応答ありWriteします。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Bond Management Peripheral: [BondManagementServer](../BondManagementServer/) example

## 動作

- 0x181EをAdvertiseする機器をscanして接続
- Bond Management Feature（0x2AA5）をReadし、対応操作のbit fieldを表示
- Bond Management Control Point（0x2AA4）へ op code 0x03（Delete bond of requesting device、LE）を**応答ありWrite**

## 主なAPI

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — 応答ありWrite

## 期待されるSerial出力

```
Bond Management Feature: 0x000011
Delete-bond op code sent
```
