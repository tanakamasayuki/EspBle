# NotifyServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

1秒ごとにカウンタ値をNotificationで送るGATT Serverです。ただしNotificationを購読しているClientが1つ以上いる間だけ送信します。[Gatt/SubscribeClient](../SubscribeClient/) exampleと組み合わせて使います。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- [Gatt/SubscribeClient](../SubscribeClient/) exampleを動かすESP32-S3 × 1（または購読できるスマートフォンGATTアプリ）

## 動作

- `begin()`前にRead + Notify可能なCharacteristicを登録します
- `onSubscriptionChanged()`でCCCD購読状態を追跡し、購読者がいる間だけ送信します
- 毎秒、増加するカウンタを文字列として送ります
- 非同期の送信失敗は`onSent()`で報告します

## 購読者がいないと送れない

Notificationは「Serverが送りたいときに送る」仕組みですが、**送っていいかどうかを決めるのはClient側**です。Notify可能なCharacteristicには**CCCD**（Client Characteristic Configuration Descriptor）という設定用のDescriptorが自動的に付き、Clientがそこへ書き込むことで購読が始まります。

つまりCCCDは**接続ごとのスイッチ**です。3台が繋がっていれば3つ独立した状態があり、購読しているのは1台だけということも普通に起こります。`notify()` は購読中の接続だけへ送り、購読していない接続には何も送りません。

このexampleが送信前に購読者数を見ているのはそのためです。誰も購読していないのに毎秒値を作って捨てるのは無駄なので、`onSubscriptionChanged()` で状態を追跡しています。**送信そのものが失敗するわけではありません**——単に届く先が無いだけです。

購読は切断で消えます。ただしClient側の設定次第で、再接続時に自動復元される場合があります（[AutoReconnectClient](../AutoReconnectClient/)）。Server側から見ると、その場合も「再接続後にCCCDが書かれた」という普通の購読として届きます。

## 主なAPI

- `EspBleGattCharacteristicConfig::notifiable` — Notify propertyとCCCDを追加します
- `gattServer.onSubscriptionChanged(callback)` — 接続ごとの`subscription.notifications` / `subscription.indications`
- `gattServer.notify(characteristic, value)` — 同期的に受理し、購読中の全接続へ送信。`mtu - 3`を超えるpayloadは`InvalidArgument`で拒否します
- `gattServer.onSent(callback)` — 非同期の送信結果（`EspBleGattSendResult`）

## 期待されるSerial出力

待機中は何も表示しません。送信中に購読Clientが消えた場合など:

```
Notification failed: ...
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[NotifyServer.ino](NotifyServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[SubscribeClient](../SubscribeClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

NotifyServer - カウンタ値を1秒ごとにNotificationで送るGATT Server。 ただしNotificationを購読しているClientがいる間だけ送る。Gatt/SubscribeClient と組み合わせる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `gattServer.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `gattServer.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `gattServer.onSubscriptionChanged(...)` — 更新を送る前に、どの接続が購読したか確認します。接続だけでCharacteristicの通知が有効になるわけではありません。
4. `gattServer.onSent(...)` — 非同期の送信結果で完了や失敗を確認します。Notifyの送信結果とIndicateの確認応答では、配送の保証範囲が異なります。
5. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
6. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。
7. `ble.gattServer().notify(...)` — 購読者への更新をキューへ登録し、受理を確認します。Notifyには受信側からのATT確認応答がないため、必要に応じ送信結果やアプリの応答を使います。

## 確認と変更のポイント

SubscribeClientと試し、保存値・購読・実際のNotifyを区別します。タイマーを実センサーへ置き換えても、送信間隔の制御と受理確認は必要です。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
