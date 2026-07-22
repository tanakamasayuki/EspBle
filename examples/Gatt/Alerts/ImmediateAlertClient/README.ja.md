# ImmediateAlertClient

> English: [README.md](README.md)

Find Meプロファイルの**locator**役。Immediate Alert Service（0x1802）へ接続し、Alert Level（0x2A06）へ**Write Without Response**で書き込んでターゲットを鳴動させます。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Central / Find Me locator）
- 1 × Immediate Alert Peripheral: [ImmediateAlertServer](../ImmediateAlertServer/) example

## 動作

- 0x1802をAdvertiseする機器をscanして接続
- 接続時に Alert Level = 2（High Alert）を Write Without Response で書き込み
- 5秒後に Alert Level = 0（No Alert）を書き込み

## 主なAPI

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, false)` — Write Without Response（最後の引数がfalse）

## 期待されるSerial出力

```
High Alert sent
No Alert sent
```
