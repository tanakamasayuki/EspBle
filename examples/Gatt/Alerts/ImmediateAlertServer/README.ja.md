# ImmediateAlertServer

> English: [README.md](README.md)

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
