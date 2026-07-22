# Scan

> English: [README.md](README.md)

継続的なactive scanを実行し、受信したadvertisementのaddress、RSSI、（存在すれば）デバイス名を表示します。Central側の最小例です。2台目のボードで[Advertise](../Advertise/) exampleを動かして組み合わせるか、周囲のBLE機器の観察に使えます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central）
- 任意の相手 — 2台目のボードで[Advertise](../Advertise/) example、または周囲の任意のBLE機器

## 動作

- 時間無制限（`durationSeconds = 0`）のactive scanを開始します
- 各Scan Resultは値型としてcopyされ、`ble.update()`のcontextでcallbackへ配送されます（BLE stack task上では実行されません）
- 全resultのaddress、RSSI、name（存在時）を表示します

## 主なAPI

- `ble.scanner().onResult(callback)` — advertisementごとに`EspBleScanResult`を受け取ります
  - `scanResult.address`、`scanResult.rssi`、`scanResult.hasName()`、`scanResult.name`
  - ほかに`advertisesService(uuid)`、`connectable`、Manufacturer Dataも参照できます
- `EspBleScanConfig` — `active`、`wantDuplicates`、`intervalMilliseconds`、`windowMilliseconds`、`durationSeconds`
- `ble.scanner().start(scanConfig)` / `ble.scanner().stop()`
- `ble.scanner().droppedResultCount()` — queue溢れで取りこぼしたresult数

## 期待されるSerial出力

```
5a:b8:1e:0c:2f:71 RSSI=-52 name=EspBle Advertiser
70:04:1d:32:99:a0 RSSI=-78
...
```
