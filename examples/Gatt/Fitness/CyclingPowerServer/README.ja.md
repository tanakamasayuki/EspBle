# CyclingPowerServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Cycling Power Service（0x1818）のPeripheralです。Cycling Power Measurement（0x2A63）を16bit flagsと符号付き16bit instantaneous power（ワット）でNotifyし、Cycling Power Feature（0x2A65）とSensor Location（0x2A5D）はReadできます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [CyclingPowerClient](../CyclingPowerClient/) example、または任意のCycling Power collector

## 動作

- Cycling Power serviceを登録し、0x1818をadvertise
- Cycling Power Feature（0x0000000C）とSensor Location = Right Crank（6）をReadable値として公開
- 1秒ごとにpowerを200〜300 Wで増加（300を超えると200へ戻る）させ、16bit flags 0x0000（optionalフィールドなし）の4byte Cycling Power MeasurementをNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Cycling Power Measurement
- `ble.gattServer().setValue(...)` — Readable な Feature / Sensor Location を設定
- `ble.gattServer().notify(...)` — 各measurementを購読者へ送信

## メモ

- instantaneous powerは16bit flagsフィールドの直後に置かれる、符号付き16bit little-endianのワット値です。

## 期待されるSerial出力

```
The server is silent; observe values on the client.
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[CyclingPowerServer.ino](CyclingPowerServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[CyclingPowerClient](../CyclingPowerClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

CyclingPowerServer - 標準Cycling Power Service（0x1818）。Cycling Power Measurement（0x2A63）を16bit flagsと符号付き16bit instantaneous power （ワット）でNotifyし、Cycling Power Feature（0x2A65）とSensor Location （0x2A5D）はReadできる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `server.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
5. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。
6. `ble.gattServer().notify(...)` — 購読者への更新をキューへ登録し、受理を確認します。Notifyには受信側からのATT確認応答がないため、必要に応じ送信結果やアプリの応答を使います。

## 確認と変更のポイント

瞬時パワーは符号付きのW値です。例を置き換える際も符号、flags、little-endianの並びを保ちます。すべて符号なしへ変換すると負値が非常に大きい値になります。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
