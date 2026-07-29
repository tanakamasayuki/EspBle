# SubscribeClient

> English: [README.md](README.md)

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
