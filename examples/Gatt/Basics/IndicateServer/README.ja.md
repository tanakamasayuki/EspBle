# IndicateServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

[NotifyServer](../NotifyServer/)のIndication版です。2秒ごとにカウンタ値を送信し、各配信はClientがATT層で確認応答します。確認結果は`onSent()`へ非同期に届きます。[Gatt/IndicateClient](../IndicateClient/)と組み合わせて使います。

値を取りこぼしてはいけない用途（状態遷移の通知など）にはIndicationを、確認応答の往復がスループットを律速するような高頻度ストリームにはNotificationを使います。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- [Gatt/IndicateClient](../IndicateClient/) exampleを動かすESP32-S3 × 1

## 動作

- `begin()`前にRead + Indicate可能なCharacteristicを登録します
- `onSubscriptionChanged()`でCCCDのIndication購読を追跡し、購読者がいる間だけ送信します
- 2秒ごとに増加するカウンタを送り、Clientが配信を確認したかを表示します

## 主なAPI

- `EspBleGattCharacteristicConfig::indicatable` — Indicate propertyとCCCDを追加します
- `gattServer.indicate(characteristic, value)` — 同期的に受理され、確認待ちでloopをblockしません
- `gattServer.onSent(callback)` — Indicationでは`result.success`がClientの配信確認を意味します
- `subscription.indications` — 接続ごとのCCCD Indication状態

## 期待されるSerial出力

```
Indication confirmed: 1
Indication confirmed: 2
...
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[IndicateServer.ino](IndicateServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[IndicateClient](../IndicateClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

IndicateServer - NotifyServerのIndication版。各値はClientからのATT確認応答つきで 配信され、結果は onSent() で届く。Gatt/IndicateClient と組み合わせる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `gattServer.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `gattServer.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `gattServer.onSubscriptionChanged(...)` — 更新を送る前に、どの接続が購読したか確認します。接続だけでCharacteristicの通知が有効になるわけではありません。
4. `gattServer.onSent(...)` — 非同期の送信結果で完了や失敗を確認します。Notifyの送信結果とIndicateの確認応答では、配送の保証範囲が異なります。
5. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
6. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。
7. `ble.gattServer().indicate(...)` — 購読者へのIndicateをキューへ登録します。ATT確認応答がありますが、即時の受理は完了より前で、相手アプリの動作完了までは保証しません。

## 確認と変更のポイント

Indicate購読側で値を確認します。キュー受理と完了を区別し、相手が処理できない速度で無制限にIndicateを積まないようにします。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
