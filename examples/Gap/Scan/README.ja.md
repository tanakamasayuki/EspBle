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

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Scan.ino](Scan.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[Advertise](../Advertise/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

Scan - 継続的なactive scanを実行し、受信したadvertisementのaddress/RSSI/nameを表示する。 Central側の最小例。Gap/Advertise exampleや周囲のBLE機器を観察できる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。

## 確認と変更のポイント

相手の名前がScan Responseにある場合はActive Scanを使います。継続的な広告が必要な場合だけ重複通知を有効にします。名前がないことや同じアドレスが繰り返し出ること自体は異常ではありません。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
