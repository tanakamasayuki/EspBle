# RunningSpeedCadenceClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Running Speed and Cadence Service（0x1814）のCentral / GATT Clientです。Sensor Location（0x2A5D）をReadし、RSC Measurement（0x2A53）のNotificationを購読して、speed・cadenceと任意のstride length・total distanceをデコードします。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- Peripheral × 1: [RunningSpeedCadenceServer](../RunningSpeedCadenceServer/) example

## 動作

- 0x1814をadvertiseする接続可能なpeerをscanして接続
- 接続時にSensor LocationをReadして表示
- RSC Measurementを購読し、瞬間speed（1/256 m/s）とcadenceをデコード。flagsのbit 2から歩行/走行を判定し、任意のstride length（bit 0、1/100 m）とtotal distance（bit 1、1/10 m）を出力

## 主なAPI

- `ble.readCharacteristic(...)` — Sensor Location を Read
- `ble.subscribe(...)` — RSC Measurement の Notification を有効化
- `ble.onNotification(callback)` — 混在幅の payload をデコード

## 期待されるSerial出力

```
Sensor location: 2
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 3.0 m
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 6.0 m
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[RunningSpeedCadenceClient.ino](RunningSpeedCadenceClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[RunningSpeedCadenceServer](../RunningSpeedCadenceServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

RunningSpeedCadenceClient - Running Speed and Cadence Service（0x1814）へ 接続し、Sensor LocationをRead、RSC MeasurementのNotificationを購読して、 speed/cadence/stride/distanceをデコードする。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.readCharacteristic(...)` — 相手の値のReadをキューへ登録します。値は即時のboolではなくonCharacteristicReadへ届きます。
4. `ble.onCharacteristicRead(...)` — 成功・UUID・値の長さを確認してからデコードします。バイナリのStringには0バイトも含まれるので、長さを使って処理します。
5. `ble.subscribe(...)` — 相手のCharacteristic更新を購読します。NotifyとIndicateの購読方式を合わせ、購読の完了を待ちます。
6. `ble.onNotification(...)` — Characteristicと接続を絞り、バイナリ形式を検証してから更新をデコードします。IndicateのデータもこのAPIへ届きます。
7. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
8. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

flagsで歩幅や総距離などの任意項目が変わります。速度・ケイデンス・距離を各項目の単位で変換し、flagを変えたらデータ長とデコード位置も合わせます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
