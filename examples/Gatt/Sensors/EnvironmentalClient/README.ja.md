# EnvironmentalClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Environmental Sensing Service（0x181A）のCentral / GATT Clientです。Temperature（0x2A6E）、Humidity（0x2A6F）、Pressure（0x2A6D）を順番にReadしてデコードし、その後Temperature Notificationを購読します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- Peripheral × 1: [EnvironmentalServer](../EnvironmentalServer/) example

## 動作

- 0x181Aをadvertiseするpeerをactive scanして接続
- 3つのcharacteristic（temperature、humidity、pressure）を順番にReadし、little-endian値（0.01 ℃、0.01 %、0.1 Pa）をデコード
- 3つ目のRead後にTemperatureを購読し、以降の変更Notificationを表示

## 主なAPI

- `ble.readCharacteristic(...)` — `onCharacteristicRead`で連鎖するRead
- `ble.subscribe(...)` / `ble.onSubscribed(...)` — Temperature Notificationの有効化と確認
- `ble.onNotification(callback)` — 変更されたTemperature値をデコード

## メモ

- temperatureとhumidityは16bit（符号付き/符号なし）、pressureは32bit値です。各デコードは使用前に受信長を検証します。

## 期待されるSerial出力

```
Temperature raw: 2150 (0.01 C units)
Humidity raw: 4875 (0.01 % units)
Pressure raw: 1013250 (0.1 Pa units)
Temperature subscription: ready
Temperature changed raw: 2175 (0.01 C units)
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[EnvironmentalClient.ino](EnvironmentalClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[EnvironmentalServer](../EnvironmentalServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.readCharacteristic(...)` — 相手の値のReadをキューへ登録します。値は即時のboolではなくonCharacteristicReadへ届きます。
2. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
3. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
4. `ble.onCharacteristicRead(...)` — 成功・UUID・値の長さを確認してからデコードします。バイナリのStringには0バイトも含まれるので、長さを使って処理します。
5. `ble.subscribe(...)` — 相手のCharacteristic更新を購読します。NotifyとIndicateの購読方式を合わせ、購読の完了を待ちます。
6. `ble.onSubscribed(...)` — result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
7. `ble.onNotification(...)` — Characteristicと接続を絞り、バイナリ形式を検証してから更新をデコードします。IndicateのデータもこのAPIへ届きます。
8. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
9. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

温度・湿度・気圧では整数の幅と倍率が異なります。例の温度はシリアル操作で変わります。実センサーへ置き換える際は、センサー出力を既存の通信形式へ変換します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
