# AlertNotificationServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Alert Notification Service（0x1811）のPeripheral。Supported New Alert Category（0x2A47）はreadableなuint16 bitmask、New Alert（0x2A46）はCategory ID＋count＋text付きの**Notification**、Alert Notification Control Point（0x2A44）はwritableです。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [AlertNotificationClient](../AlertNotificationClient/) example、または Alert Notification client

## 動作

- `begin()`の前にcategory bitmask、New Alert、Control Pointを登録し、0x1811をAdvertise
- Email（bit 1）とSMS/MMS（bit 5）カテゴリ対応（0x0022）を提示
- 「Notify New Alert Immediately」（command 2）のControl Point書き込みで、要求されたカテゴリのNew Alert（count 3、text "Bob"）をNotify

## 主なAPI

- `ble.gattServer().onWritten(...)` — Control Pointコマンドを受信
- `ble.gattServer().notify(...)` — New AlertをNotify

## 期待されるSerial出力

```
Notify New Alert for category 1
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[AlertNotificationServer.ino](AlertNotificationServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[AlertNotificationClient](../AlertNotificationClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

AlertNotificationServer - 標準Alert Notification Service（0x1811）。 Supported New Alert Category（0x2A47）はreadableなuint16 bitmask、New Alert （0x2A46）はCategory ID＋count＋text付きでNotify、Alert Notification Control Point（0x2A44）はwritable。「Notify New Alert Immediately」（command 2）で New AlertのNotificationを発火する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `server.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `server.onWritten(...)` — updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
5. `ble.gattServer().notify(...)` — 購読者への更新をキューへ登録し、受理を確認します。Notifyには受信側からのATT確認応答がないため、必要に応じ送信結果やアプリの応答を使います。
6. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
7. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

対応カテゴリのbitmask、Control Pointコマンド、New Alertデータは別の役割です。有効なカテゴリIDを使い、購読後に要求する順序を保ちます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
