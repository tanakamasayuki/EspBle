# IndicateClient

> English: [README.md](README.md)

[Gatt/IndicateServer](../IndicateServer/) exampleへ接続し、Indicationを購読します。[SubscribeClient](../SubscribeClient/)とのAPI上の違いは`subscribe()`の最終引数だけで、`false`を渡すとCCCDへNotificationビットではなくIndicationビット（0x0002）を書き込みます。受信した各値はスタックが自動で確認応答し、これによってServer側の`onSent()`が「配信確認済み」を報告します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [Gatt/IndicateServer](../IndicateServer/) exampleを動かすESP32-S3 × 1

## 動作

- IndicateServerのService UUIDをscanして接続します
- 接続完了直後にIndicationを購読します
- 受信した各値を`notification.indication`で判別して`Indication`ラベルつきで表示します

## 主なAPI

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid, false)` — Indicationの購読
- `ble.onNotification(callback)` — NotificationとIndicationの両方を受信します。`notification.indication`で区別できます
- `ble.onSubscribed(callback)` — CCCD書込み完了

## 期待されるSerial出力

```
Indication: 1
Indication: 2
...
```
