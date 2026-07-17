# SubscribeClient

> English: [README.md](README.md)

[Gatt/NotifyServer](../NotifyServer/) exampleへ接続し、Notification Characteristicを購読して受信値をすべて表示します。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [Gatt/NotifyServer](../NotifyServer/) exampleを動かすESP32-S3 × 1

## 動作内容

- NotifyServerのService UUIDをscanして接続します
- 接続完了直後にNotificationを購読します
- 購読完了の結果と、受信した各Notification payloadを表示します

## 主なAPI

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid, notifications)` — `true`でNotification、`false`でIndicationを購読（CCCDへ書込み）
- `ble.onSubscribed(callback)` — CCCD書込み完了（`result.success`）
- `ble.onNotification(callback)` — `connectionId`、UUID、copy済みpayload、indicationフラグを持つ`EspBleGattNotification`
- `ble.unsubscribe(connectionId, serviceUuid, characteristicUuid)` — CCCDを解除します

## 期待されるSerial出力

```
Notification: 1
Notification: 2
Notification: 3
...
```
