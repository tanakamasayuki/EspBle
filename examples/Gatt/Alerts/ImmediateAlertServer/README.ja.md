# ImmediateAlertServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Immediate Alert Service（0x1802）のPeripheral — Find Meプロファイルの**ターゲット**役。Alert Level（0x2A06）は**Write Without Response**のuint8（0 = No Alert、1 = Mild、2 = High）1つだけです。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral / Find Meターゲット）
- 1 × Central: [ImmediateAlertClient](../ImmediateAlertClient/) example、または Find Me locator

## 動作

- `begin()`の前にAlert Levelをwrite / write-without-responseなCharacteristicとして登録し、0x1802をAdvertise
- 書かれたAlert Levelごとに`onWritten`で反応（実機のターゲットなら鳴動・振動する）

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .writable = true, .writableWithoutResponse = true })`
- `ble.gattServer().onWritten(...)` — Alert Levelの書き込みをloop contextで受信

## 期待されるSerial出力

```
Alert Level: 2 (High Alert)
Alert Level: 0 (No Alert)
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
