# NotifyServer

> English: [README.md](README.md)

1秒ごとにカウンタ値をNotificationで送るGATT Serverです。ただしNotificationを購読しているClientが1つ以上いる間だけ送信します。[Gatt/SubscribeClient](../SubscribeClient/) exampleと組み合わせて使います。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- [Gatt/SubscribeClient](../SubscribeClient/) exampleを動かすESP32-S3 × 1（または購読できるスマートフォンGATTアプリ）

## 動作

- `begin()`前にRead + Notify可能なCharacteristicを登録します
- `onSubscriptionChanged()`でCCCD購読状態を追跡し、購読者がいる間だけ送信します
- 毎秒、増加するカウンタを文字列として送ります
- 非同期の送信失敗は`onSent()`で報告します

## 主なAPI

- `EspBleGattCharacteristicConfig::notifiable` — Notify propertyとCCCDを追加します
- `gattServer.onSubscriptionChanged(callback)` — 接続ごとの`subscription.notifications` / `subscription.indications`
- `gattServer.notify(serviceUuid, characteristicUuid, value)` — 同期的に受理し、購読中の全接続へ送信。`mtu - 3`を超えるpayloadは`InvalidArgument`で拒否します
- `gattServer.onSent(callback)` — 非同期の送信結果（`EspBleGattSendResult`）

## 期待されるSerial出力

待機中は何も表示しません。送信中に購読Clientが消えた場合など:

```
Notification failed: ...
```
