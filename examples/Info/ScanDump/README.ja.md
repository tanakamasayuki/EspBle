# ScanDump

> English: [README.md](README.md)

EspBleが各advertisementから取り出す全フィールドをダンプする診断用スキャナです: address・address種別、RSSI、connectable/scannableフラグ、name、全Service UUID、Manufacturer Dataのhex表示。scan filterを書く前に相手が実際に何をadvertiseしているかを確認したり、`advertisesService()`が一致しない原因を調べたりするのに使います。

## ハードウェア

- ESP32-S3 × 1（またはEspBle対応ボード）

## 動作内容

- 継続的なactive scanを実行します（scan responseも取得するため、nameが見える機器が増えます）
- advertisementごとに全フィールドを1行で表示します
- `q`で診断カウンタ（scan result / eventの取りこぼし数）を表示します

## Serialコマンド

| コマンド | 動作 |
|---------|------|
| `q` | `droppedScanResults` / `droppedEvents`カウンタを表示 |

## 主なAPI

- `EspBleScanResult` — `address`、`addressType`、`rssi`、`connectable`、`scannable`、`name`、`serviceUuids[]` / `serviceUuidCount`、`manufacturerData`
- `scanResult.hasName()` / `hasManufacturerData()`
- `ble.scanner().droppedResultCount()` — queue溢れで失われたscan result数
- `ble.droppedEventCount()` — queue溢れで失われた接続イベント数

## 期待されるSerial出力

```
Scanning. Send 'q' to print diagnostic counters.
5a:b8:1e:0c:2f:71 type=0 rssi=-52 connectable name="EspBle Keyboard" uuid=1812 uuid=180f
70:04:1d:32:99:a0 type=1 rssi=-78 connectable manufacturer[8]=4c0010050b1c72a1
counters: droppedScanResults=0 droppedEvents=0
```
