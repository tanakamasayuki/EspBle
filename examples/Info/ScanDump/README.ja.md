# ScanDump

> English: [README.md](README.md)

EspBleが各advertisementから取り出す全フィールドをダンプする診断用スキャナです: address・address種別、RSSI、connectable/scannableフラグ、name、全Service UUID、Service Data、Manufacturer Dataのhex表示。iBeacon payloadはUUID / major / minor / measured powerへデコードします。scan filterを書く前に相手が実際に何をadvertiseしているかを確認したり、`advertisesService()`が一致しない原因を調べたりするのに使います。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central）
- 調べたい周囲の任意のBLE機器

## 動作

- 継続的なactive scanを実行します（scan responseも取得するため、nameが見える機器が増えます）
- advertisementごとに全フィールドを1行で表示します
- Manufacturer DataがiBeaconのレイアウト（Apple company ID `0x004C`）に一致する場合はデコードします
- `q`を送ると診断カウンタ（`droppedScanResults` / `droppedEvents`）を表示します
- `d`を送ると重複報告のon/offを切り替えて再scanします

## 主なAPI

- `EspBleScanResult` — `address`、`addressType`、`rssi`、`connectable`、`scannable`、`name`、`serviceUuids[]` / `serviceUuidCount`、`manufacturerData`、`serviceData[]` / `serviceDataCount`、`appearance`、`txPowerLevel`
- `scanResult.hasName()` / `hasManufacturerData()` / `hasServiceData()` / `hasAppearance()` / `hasTxPowerLevel()`
- `EspBleIBeacon.h` の `espBleDecodeIBeacon()` — iBeacon manufacturer dataのデコード
- `EspBleScanConfig::wantDuplicates` — falseなら機器ごとに1回だけ報告、trueなら受信したadvertisementをすべて報告する
- `ble.scanner().droppedResultCount()` — queue溢れで失われたscan result数
- `ble.droppedEventCount()` — queue溢れで失われた接続イベント数

## 重複報告

既定（`wantDuplicates = false`）では、**1つの機器につき1回しか報告されません**。同じ機器のadvertisementは何度も飛んできますが、2回目以降は捨てられます。周囲の機器を一覧するだけなら、このほうが読みやすいためです。

ただし**payloadが変化し続ける機器では最初の値しか見えません**。センサービーコンの値の変化を追いたい場合は `d` で重複報告を有効にしてください。設定はscan開始時に反映されるため、切り替えると自動でscanを開始し直します。

```
Scanning. duplicates=off
（Service Dataを5秒ごとに更新する機器でも、報告は1回きり）

Scanning. duplicates=on
（更新のたびに新しい値が届く）
```

## 期待されるSerial出力

```
Scanning. duplicates=off
Commands: q counters, d toggle duplicate reporting
5a:b8:1e:0c:2f:71 type=0 rssi=-52 connectable name="EspBle Keyboard" uuid=1812 uuid=180f
d0:cf:13:58:fd:95 type=0 rssi=-14 connectable scannable name="EspBle Scan Response" appearance=0x0341 txpower=9dBm loss=23dB uuid=5266f727-49d7-4eaf-a6f1-7363616e7270 manufacturer[5]=ffff010203
70:04:1d:32:99:a0 type=1 rssi=-78 connectable manufacturer[8]=4c0010050b1c72a1
d0:cf:13:58:fd:95 type=0 rssi=-13 uuid=0000181a-0000-1000-8000-00805f9b34fb servicedata[0000181a-0000-1000-8000-00805f9b34fb][2]=c409
d0:cf:13:58:fd:95 type=0 rssi=-13 manufacturer[25]=4c0002150102030405060708090a0b0c0d0e0f1000640001c5 ibeacon uuid=01020304-0506-0708-090a-0b0c0d0e0f10 major=100 minor=1 power=-59
counters: droppedScanResults=0 droppedEvents=0
```
