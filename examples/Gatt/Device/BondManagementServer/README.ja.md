# BondManagementServer

> English: [README.md](README.md)

標準Bond Management Service（0x181E）のPeripheral。Bond Management Feature（0x2AA5）は対応操作のread可能なuint24 bit field、Bond Management Control Point（0x2AA4）はwritableでop codeを`onWritten`で受け取ります。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [BondManagementClient](../BondManagementClient/) example、または Bond Management client

## 動作

- `begin()`の前にControl PointとFeatureを登録し、0x181EをAdvertise
- 「Delete bond of requesting device（LE）」（bit 0）と「全bond削除（LE + BR/EDR）」（bit 4）対応を提示 → 0x000011
- op code 0x03（Delete bond of requesting device、LE）で、そのpeerのbondを切断後に削除（アクティブなリンクを壊さないよう遅延）

## 主なAPI

- `ble.gattServer().onWritten(...)` — Control Pointのop codeを受信
- `ble.bondCount()` / `ble.bond(i, bond)` / `ble.deleteBond(bond)` — bondの列挙と削除

## 期待されるSerial出力

```
Bond Management op code: 3
Bond deleted
```
