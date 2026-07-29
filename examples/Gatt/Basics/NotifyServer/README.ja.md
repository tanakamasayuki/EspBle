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
