# ReferenceTimeUpdateServer

> English: [README.md](README.md)

標準Reference Time Update Service（0x1806）のPeripheral。Time Update Control Point（0x2A16）は**Write Without Response**（1 = Get Reference Update、2 = Cancel Reference Update）、Time Update State（0x2A17）はread可能な2バイト値（Current State＋Result）です。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [ReferenceTimeUpdateClient](../ReferenceTimeUpdateClient/) example、または Reference Time Update client

## 動作

- `begin()`の前にControl PointとStateを登録し、0x1806をAdvertise
- 初期状態はIdle・Successful（0, 0）
- Control Point書き込みで、Get Reference Update（1）はStateをUpdate Pending（1, 0）へ遷移、Cancel Reference Update（2）はCanceled結果でIdle（0, 1）へ戻す

## 主なAPI

- `ble.gattServer().onWritten(...)` — Control Pointコマンドを受信
- `ble.gattServer().setValue(...)` — read専用Stateを更新

## 期待されるSerial出力

```
Get Reference Update
Cancel Reference Update
```
