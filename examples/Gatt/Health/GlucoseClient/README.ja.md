# GlucoseClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Glucose Service（0x1808）へ接続し、Glucose MeasurementのNotificationとRecord Access Control Point（RACP）のIndicationを購読し、「Report Stored Records（all）」を書き込んで、デコードしたレコードとRACP応答を表示します。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Glucose Peripheral: [GlucoseServer](../GlucoseServer/) example または市販の血糖値計

## 動作

- 0x1808をAdvertiseする機器をscanして接続
- Glucose Measurement（0x2A18）のNotificationとRACP（0x2A52）のIndicationを購読
- RACP「Report Stored Records（all）」要求（opcode 1、operator 1）を書込み
- Notifyされた各Measurement（sequence番号＋SFLOAT濃度）とRACP応答をデコード

## 主なAPI

- `ble.subscribe(..., true)` / `ble.subscribe(..., false)` — MeasurementはNotification、RACPはIndication
- `ble.writeCharacteristic(..., true)` — RACP要求の確認応答付きWrite

## 期待されるSerial出力

```
Glucose record #1: 99
RACP response: request=1 status=1
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[GlucoseClient.ino](GlucoseClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[GlucoseServer](../GlucoseServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

GlucoseClient - Glucose Service（0x1808）へ接続し、Glucose Measurementの NotificationとRecord Access Control Point（RACP）のIndicationを購読し、 「Report Stored Records（all）」を書き込んで、デコードした測定値とRACP応答を 表示する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.subscribe(...)` — 相手のCharacteristic更新を購読します。NotifyとIndicateの購読方式を合わせ、購読の完了を待ちます。
4. `ble.onSubscribed(...)` — result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
5. `ble.writeCharacteristic(...)` — 相手へのWriteをキューへ登録し、完了イベントを確認します。Write Without Responseには、相手アプリの処理を保証するATT応答はありません。
6. `ble.onNotification(...)` — Characteristicと接続を絞り、バイナリ形式を検証してから更新をデコードします。IndicateのデータもこのAPIへ届きます。
7. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
8. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

RACPは購読→保存記録要求→測定値更新→Control PointのIndicateという手順です。1件のデモ記録を置き換える際も状態遷移と完了順序を保ちます。Notifyの送信結果だけでは、相手アプリでの受理までは確認できません。

Serverの値はプロトコルを示す模擬値で、医療センサーの実測値ではありません。実データの取得は別に置き換え、サービスの符号化とClient側の検証を保ちます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
