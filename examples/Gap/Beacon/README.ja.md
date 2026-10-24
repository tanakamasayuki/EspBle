# Beacon

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

manufacturer dataを載せたnon-connectable・non-scannableなbeaconをbroadcastします。connectableなPeripheralである[Advertise](../Advertise/)と違い、これは純粋なbroadcasterです。Centralから接続もscanもされず、設定した間隔でadvertising payloadを送信するだけです。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（broadcaster）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect等のスキャナアプリ

## 動作

- manufacturer data（ここではcompany ID `0xFFFF` ＋小さなpayload）をnon-connectable・non-scannableなadvertisementとしてbroadcastします
- 設定した間隔で送信するだけです。スキャナからは`connectable = false`・`scannable = false`として見えます

## 主なAPI

- `ble.advertising().setConnectable(false)` — non-connectableモード（GATT接続不可）
- `ble.advertising().setScanResponseEnabled(false)` — non-scannable（純粋なbroadcaster。scan responseなし）
- `ble.advertising().setManufacturerData(data, length)` — broadcastするpayload
- `ble.advertising().setInterval(minMs, maxMs)` — advertising間隔（ミリ秒。20〜10240。non-connectableでは仕様上100 ms以上が必要）
- `ble.advertising().start()` — broadcast開始

## メモ

- manufacturer dataは必要に応じて自社の割当company IDやiBeaconレイアウトに置き換える。31 byteのlegacy advertising制限が適用される。
- 通常のconnectableなPeripheralにするには`setConnectable`を既定値（`true`）のままにする。

## 期待されるSerial出力

成功時は何も表示しません。失敗時:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Beacon.ino](Beacon.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[ScanDump](../../Info/ScanDump/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

Beacon - manufacturer dataを載せたnon-connectable・non-scannableなbeaconを broadcastする。setConnectable(false) で純粋なbroadcaster（GATT接続不可）に、 setScanResponseEnabled(false) でnon-scannableに、setInterval() で送信間隔を 制御する。Gap/Scan exampleや汎用スキャナアプリで受信できる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。

## 確認と変更のポイント

Manufacturer Dataは会社識別子を含むバイナリです。長さと形式を明示します。接続不可のビーコンはGATT Readではなくスキャンで観察します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
