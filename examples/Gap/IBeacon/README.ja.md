# IBeacon

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

Apple iBeaconをbroadcastします。non-connectable・non-scannableなadvertisementのmanufacturer dataに、proximity UUID・major・minor・measured powerを載せます。レイアウトはbackend非依存の`EspBleIBeacon.h` codecで組み立てます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（broadcaster）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect / Locate Beacon等のbeaconアプリ

## 動作

- `espBleEncodeIBeacon(...)`で25 byteのiBeacon manufacturer data（Apple company ID `0x004C`、type `0x02`、length `0x15`、UUID、major、minor、measured power）を構築します
- 設定した間隔で純粋なnon-scannable advertisementとしてbroadcastします

## 主なAPI

- `EspBleIBeaconData` — proximity `uuid[16]`、`major`、`minor`、`measuredPower`
- `espBleEncodeIBeacon(beacon, out)` — `EspBleIBeaconManufacturerDataSize`（25）byteへencode
- `espBleDecodeIBeacon(manufacturerData, length, beacon)` / `espBleIsIBeacon(...)` — スキャナ側でdecode/検証
- `ble.advertising().setConnectable(false)` / `setScanResponseEnabled(false)` / `setManufacturerData(...)` / `setInterval(...)`

## メモ

- `measuredPower`は1 m時の校正RSSI（通常は負値、例: -59）で、受信側が距離推定に使います。
- UUID / major / minorは自分の値に置き換えてください。

## 期待されるSerial出力

成功時は何も表示しません。失敗時:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[IBeacon.ino](IBeacon.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[ScanDump](../../Info/ScanDump/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

IBeacon - Apple iBeaconをbroadcastする。iBeaconレイアウト（proximity UUID ＋major＋minor＋measured power）はbackend非依存のEspBleIBeacon.h codecで 組み立て、non-connectable・non-scannableなadvertisementのmanufacturer data として送信する。Gap/Scan exampleやbeaconアプリで受信できる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。

## 確認と変更のポイント

UUID・major・minorで例のビーコンを識別します。Measured Powerは校正用の値で、測定された距離ではありません。ScanDumpでデータの符号化を確認します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
