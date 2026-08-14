# AutoReconnectClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

[Gatt/NotifyServer](../NotifyServer/) exampleへ接続し、一度だけ購読します。auto-reconnectを有効にすると、persistent subscription（既定on）と併せて、想定外の切断後にリンクと購読が自動復元され、追加コードなしでNotificationが再開します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [Gatt/NotifyServer](../NotifyServer/) exampleを動かすESP32-S3 × 1

## 動作

- NotifyServerのService UUIDをscanして接続します
- 一度だけ購読し、`setAutoReconnect(true)`を呼びます
- 想定外の切断時、ライブラリが同じpeer addressへ自動再接続し、購読も自動で復元（persistent subscription）するため、Notificationが再開します

## 主なAPI

- `ble.setAutoReconnect(true)` — connect済みの全Central peerを記憶し、想定外の切断時に自動再接続する。`disconnect()`は意図的な切断として再接続対象から除外する
- `EspBleConfig::persistentSubscriptions` — 既定on。`subscribe()`成功をpeerごとに記憶し、再接続時に復元する
- `ble.onConnected` / `ble.onDisconnected` / `ble.onSubscribed` / `ble.onNotification`

## 期待されるSerial出力

```
Connected to ...
Subscription active (restored automatically after a reconnect).
Notification: 1
Notification: 2
Disconnected - auto-reconnect will restore the link.
Connected to ...
Subscription active (restored automatically after a reconnect).
Notification: 3
...
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
