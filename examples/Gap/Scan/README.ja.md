# Scan

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

継続的なactive scanを実行し、受信したadvertisementのaddress、RSSI、（存在すれば）デバイス名を表示します。Central側の**最小例**です。2台目のボードで[Advertise](../Advertise/) exampleを動かして組み合わせるか、周囲のBLE機器の観察に使えます。

ここで表示するのは3項目だけです。Service UUID・Service Data・Manufacturer Data・iBeaconまで含めて**全フィールドを見たい場合は[Info/ScanDump](../../Info/ScanDump/)**を使ってください。このexampleは「スキャンを始めて結果を受け取る」最小の書き方を示すことに絞っています。

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
- `EspBleScanConfig` — `active`、`wantDuplicates`、`intervalMilliseconds`、`windowMilliseconds`、`durationSeconds`、`acceptListOnly`
  - `acceptListOnly = true` にすると、`ble.addToAcceptList()` で登録した相手のアドバタイズだけを受け取ります。それ以外はコントローラが捨てるので`onResult`まで届きません（[Gap/AcceptList](../AcceptList/)は同じリストを接続の制限に使う例です）。照合はアドレス単位なので、RPAを回転させる相手はbonding後でないと登録できません
- `ble.scanner().start(scanConfig)` / `ble.scanner().stop()`
- `ble.scanner().droppedResultCount()` — queue溢れで取りこぼしたresult数

## 期待されるSerial出力

```
5a:b8:1e:0c:2f:71 RSSI=-52 name=EspBle Advertiser
70:04:1d:32:99:a0 RSSI=-78
...
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続
