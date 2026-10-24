# ScanDump

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

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

## 関連するガイド

- [BLE入門ガイド §1 BLEとは](../../../docs/GUIDE_BLE_BASICS.ja.md#1-bleとは) — 以降の文書が使う語彙

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[ScanDump.ino](ScanDump.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

ScanDump - EspBleが各advertisementから取り出す全フィールド（UUID表記・nameの有無・ Manufacturer Data・Service Data）をダンプし、iBeacon payloadはデコードする診断用 スキャナ。scan filterを書く前に相手が実際に何をadvertiseしているか確認するのに使う。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。

## 確認と変更のポイント

解析済みの広告項目を調べる例で、RFの生パケットを取得するスニファではありません。ビーコンの変化が止まったと判断する前に、重複設定と結果の破棄数を確認します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
