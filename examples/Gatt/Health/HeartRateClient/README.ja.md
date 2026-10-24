# HeartRateClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Heart Rate Service（0x180D）をAdvertiseするPeripheralへ接続し、Body Sensor LocationをReadして、Heart Rate MeasurementのNotificationを購読します。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Heart Rate Peripheral: [HeartRateServer](../HeartRateServer/) example、または市販の心拍計

## 動作

- 0x180DをAdvertiseする機器をscanして接続
- Body Sensor Location（0x2A38）をRead
- Heart Rate Measurement（0x2A37）のNotificationを購読
- Measurementをflagsに従ってデコード: 8/16-bit心拍数、任意のEnergy Expended、複数のRR-Intervalを、可変長の境界を検証しながら処理

## 主なAPI

- `ble.readCharacteristic(connectionId, service, characteristic)` — Body Sensor LocationをRead
- `ble.subscribe(connectionId, service, characteristic)` — Notificationを購読
- `ble.onNotification(...)` — 各Heart Rate Measurementを受信

## 期待されるSerial出力

```
Body Sensor Location: 1
Heart Rate subscription: ready
Heart Rate: 71 bpm, RR intervals: 1 (first: 1024/1024 s)
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HeartRateClient.ino](HeartRateClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[HeartRateServer](../HeartRateServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.readCharacteristic(...)` — 相手の値のReadをキューへ登録します。値は即時のboolではなくonCharacteristicReadへ届きます。
4. `ble.onCharacteristicRead(...)` — 成功・UUID・値の長さを確認してからデコードします。バイナリのStringには0バイトも含まれるので、長さを使って処理します。
5. `ble.subscribe(...)` — 相手のCharacteristic更新を購読します。NotifyとIndicateの購読方式を合わせ、購読の完了を待ちます。
6. `ble.onSubscribed(...)` — result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
7. `ble.onNotification(...)` — Characteristicと接続を絞り、バイナリ形式を検証してから更新をデコードします。IndicateのデータもこのAPIへ届きます。
8. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
9. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

flagsにより心拍数の幅と任意項目が変わります。Heart Rate Measurementが常にflags 1バイト＋心拍数1バイトとは考えないようにします。

Serverの値はプロトコルを示す模擬値で、医療センサーの実測値ではありません。実データの取得は別に置き換え、サービスの符号化とClient側の検証を保ちます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
