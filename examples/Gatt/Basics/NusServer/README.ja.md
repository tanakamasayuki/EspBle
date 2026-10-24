# NusServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Nordic UART Service（NUS）のUUID構成を汎用GATT Server APIで実装します（Peripheral）。Service `6e400001-…` のもとで、RX（`6e400002-…`）はWriteを受け取り、TX（`6e400003-…`）はNotificationを送ります。受信したRXデータは購読中のTX Clientへechoします。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- [NusClient](../NusClient/) exampleを動かすESP32-S3 × 1（またはNUS互換のCentral）

## 動作

- `begin()`前にNUS Serviceを登録し、RX Characteristicを応答あり/なしWrite可、TX CharacteristicをNotify可として構成します
- RX Writeを表示し、同じバイト列をTX Notificationとしてecho、echoが受理されたかを表示します
- TXの購読状態変化を`onSubscriptionChanged()`で報告します
- `EspBle NUS`の名前でNUS Service UUIDをadvertiseします

## 主なAPI

- `gattServer.addCharacteristic(service, uuid, config)` — RXは`writable` + `writableWithoutResponse`、TXは`notifiable`。返るハンドルで以降を操作します
- `gattServer.onWritten(callback)` — `characteristicUuid.equalsIgnoreCase(...)`でRX UUIDに絞り込む`EspBleGattWrite`
- `gattServer.notify(characteristic, value)` — 受信バイトをTXでechoし、受理されたかを返します
- `gattServer.onSubscriptionChanged(callback)` — TX Characteristicの`subscription.notifications`

## メモ

- NUSはpacket指向のGATT慣例であり、Bluetooth Classic SPPではありません。payloadは接続のATT/MTU上限内に収め、複数packetへ分割する場合はapplication側でframingします。

## 期待されるSerial出力

```
TX notifications: 1
RX: hello
Echo accepted: 1
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[NusServer.ino](NusServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[NusClient](../NusClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `server.onWritten(...)` — updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
4. `ble.gattServer().notify(...)` — 購読者への更新をキューへ登録し、受理を確認します。Notifyには受信側からのATT確認応答がないため、必要に応じ送信結果やアプリの応答を使います。
5. `server.onSubscriptionChanged(...)` — 更新を送る前に、どの接続が購読したか確認します。接続だけでCharacteristicの通知が有効になるわけではありません。
6. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
7. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

NUSは2つのCharacteristicで、自動的なシリアルストリームではありません。例はシリアルの行を使います。バイナリや長いメッセージには区切りと分割方法を設計し、購読成功を準備完了として扱います。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
