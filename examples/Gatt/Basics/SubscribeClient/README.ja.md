# SubscribeClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

[Gatt/NotifyServer](../NotifyServer/) exampleへ接続し、Notification Characteristicを購読して受信値をすべて表示します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [Gatt/NotifyServer](../NotifyServer/) exampleを動かすESP32-S3 × 1

## 動作

- NotifyServerのService UUIDをscanして接続します
- 接続完了直後にNotificationを購読します
- 購読完了の結果と、受信した各Notification payloadを表示します

## 主なAPI

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid, notifications)` — `true`でNotification、`false`でIndicationを購読（CCCDへ書込み）
- `ble.onSubscribed(callback)` — CCCD書込み完了（`result.success`）
- `ble.onNotification(callback)` — `connectionId`、UUID、copy済みpayload、indicationフラグを持つ`EspBleGattNotification`
- `ble.unsubscribe(connectionId, serviceUuid, characteristicUuid)` — CCCDを解除します

## 注意

- **購読はCCCDへの書き込みです。** `subscribe()` は相手のCharacteristicに付いているCCCDへビットを書き、その完了が `onSubscribed()` に届きます。無線のやり取りを伴うので、`subscribe()` が `true` を返した時点ではまだ購読していません。
- **`onNotification()` はNotificationとIndicationの両方を受け取ります。** `notification.indication` で区別してください（[IndicateClient](../IndicateClient/)）。
- **切断後の再接続では、購読が自動で復元されます。** `EspBleConfig::persistentSubscriptions` が既定でonのため、`subscribe()` を呼び直さなくても `onSubscribed()` が発火します（[AutoReconnectClient](../AutoReconnectClient/)）。手動で管理したい場合はこれをoffにしてください。

## 期待されるSerial出力

```
Notification: 1
Notification: 2
Notification: 3
...
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[SubscribeClient.ino](SubscribeClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[NotifyServer](../NotifyServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

SubscribeClient - Gatt/NotifyServer example へ接続し、Notificationを購読して 受信値を表示する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.subscribe(...)` — 相手のCharacteristic更新を購読します。NotifyとIndicateの購読方式を合わせ、購読の完了を待ちます。
4. `ble.onSubscribed(...)` — result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
5. `ble.onNotification(...)` — Characteristicと接続を絞り、バイナリ形式を検証してから更新をデコードします。IndicateのデータもこのAPIへ届きます。
6. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
7. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

相手でNotifyServerを動かし、購読成功を待ってから周期値を確認します。相手がIndicateを使う場合は購読方式も合わせます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
