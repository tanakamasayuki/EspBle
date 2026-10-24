# GlucoseServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

**Record Access Control Point（RACP）**手続きを持つ標準Glucose Service（0x1808）のPeripheral。Clientが「Report Stored Records（all）」を書き込むと、Glucose Measurementを1件Notifyし、続けてRACP応答をIndicateします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [GlucoseClient](../GlucoseClient/) example、または Glucose collector

## 動作

- Glucose Measurement（0x2A18, notify）、Glucose Feature（0x2A51, read）、RACP（0x2A52, write + indicate）を登録
- RACP「Report Stored Records」書込みで、Measurement（sequence番号、base time、SFLOAT濃度、type/location）を1件Notify
- 送信はqueueされるが、Measurementの送信結果イベントを受けてからRACP応答をIndicateするため`onSent`で順次実行する

## 主なAPI

- `ble.gattServer().onWritten(...)` — RACP要求を受信
- `ble.gattServer().onSent(...)` — notify → indicate手続きを順次実行
- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 16-bit SFLOAT濃度

## 期待されるSerial出力

Server側は出力しません。レコードはClientで確認します。

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[GlucoseServer.ino](GlucoseServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[GlucoseClient](../GlucoseClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

GlucoseServer - Record Access Control Point（RACP）付きの標準Glucose Service（0x1808）。Clientが「Report Stored Records（all）」を書き込むと、 Glucose Measurement（sequence、base time、SFLOAT濃度）を1件Notifyし、続けて RACP応答をIndicateする。送信はFIFOにqueueされるので、ここでonSentを使うのは 意図的で、measurementの送信結果イベントを受けてからRACPの「完了」応答をIndicateする。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `server.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `server.onWritten(...)` — updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
5. `ble.gattServer().notify(...)` — 購読者への更新をキューへ登録し、受理を確認します。Notifyには受信側からのATT確認応答がないため、必要に応じ送信結果やアプリの応答を使います。
6. `server.onSent(...)` — 非同期の送信結果で完了や失敗を確認します。Notifyの送信結果とIndicateの確認応答では、配送の保証範囲が異なります。
7. `ble.gattServer().indicate(...)` — 購読者へのIndicateをキューへ登録します。ATT確認応答がありますが、即時の受理は完了より前で、相手アプリの動作完了までは保証しません。
8. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
9. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

RACPは購読→保存記録要求→測定値更新→Control PointのIndicateという手順です。1件のデモ記録を置き換える際も状態遷移と完了順序を保ちます。Notifyの送信結果だけでは、相手アプリでの受理までは確認できません。

Serverの値はプロトコルを示す模擬値で、医療センサーの実測値ではありません。実データの取得は別に置き換え、サービスの符号化とClient側の検証を保ちます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)

この最小例の `onSent` はCharacteristicと状態を見て次へ進み、`result.success` による分岐は省略しています。実用の記録転送では、Notify／Indicateの受理失敗や非同期失敗を処理し、失敗時に成功のRACP応答を送らない状態遷移を追加します。
