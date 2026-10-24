# RunningSpeedCadenceServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Running Speed and Cadence Service（0x1814）のPeripheralです。RSC Measurement（0x2A53）を瞬間speed・cadenceと任意のstride length・total distanceでNotifyし、RSC Feature（0x2A54）とSensor Location（0x2A5D）はReadできます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [RunningSpeedCadenceClient](../RunningSpeedCadenceClient/) example、または任意のRSC collector

## 動作

- RSC serviceを登録し、0x1814をadvertise
- RSC Feature = Stride + Distance（0x0003）とSensor Location = In Shoe（2）をReadable値として公開
- 1秒ごとに、3.0 m/s・cadence 180・stride 1.25 m・増加するtotal distance（+3.0 m）を10byte RSC Measurement（flags 0x03）としてNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — RSC Measurement
- `ble.gattServer().setValue(...)` — Readable な Feature / Sensor Location を設定
- `ble.gattServer().notify(...)` — 各measurementを購読者へ送信

## メモ

- speedは1/256 m/s、stride lengthは1/100 m、total distanceは1/10 m単位です。flagsのbit 2（歩行/走行）は0のままなので、測定値は歩行として報告されます。

## 期待されるSerial出力

```
The server is silent; observe values on the client.
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[RunningSpeedCadenceServer.ino](RunningSpeedCadenceServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[RunningSpeedCadenceClient](../RunningSpeedCadenceClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

RunningSpeedCadenceServer - 標準Running Speed and Cadence Service （0x1814）。RSC Measurement（0x2A53）を瞬間speed・cadenceと任意のstride length・total distanceでNotifyし、RSC Feature（0x2A54）とSensor Location （0x2A5D）はReadできる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `server.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
5. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。
6. `ble.gattServer().notify(...)` — 購読者への更新をキューへ登録し、受理を確認します。Notifyには受信側からのATT確認応答がないため、必要に応じ送信結果やアプリの応答を使います。

## 確認と変更のポイント

flagsで歩幅や総距離などの任意項目が変わります。速度・ケイデンス・距離を各項目の単位で変換し、flagを変えたらデータ長とデコード位置も合わせます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
