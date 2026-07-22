# Mtu

> English: [README.md](README.md)

接続前に大きめのATT MTUを希望値として設定し、交換結果を観察します。希望MTUは`begin()`へ渡すconfigで指定し、同梱NimBLE backendが接続確立時に交換します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central）
- BLE Peripheral × 1 — このsketchは[Gatt/Basics/NotifyServer](../../Gatt/Basics/NotifyServer/) exampleのService UUIDをscanするので、2台目のボードでNotifyServerを動かしてください

## 動作

- `begin()`前に`config.preferredMtu = 185`を設定します
- NotifyServerのService UUIDをadvertiseする最初の相手へ接続します
- 交換されたMTUと、そこから決まるNotification payload上限（`mtu - 3`）を表示します
- MTU変更イベントを変更前後の値と一緒に表示します

## 主なAPI

- `EspBleConfig::preferredMtu` — 希望ATT MTU（23〜517）。範囲外は`begin()`が`InvalidArgument`で拒否します
- `connection.mtu` — 接続完了時に取得したMTUのsnapshot
- `connection.maximumNotificationPayload()` — `mtu - 3`（ATT notification header分を除いた値）
- `ble.onMtuChanged(callback)` — `event.previousMtu`と`event.connection.mtu`

## メモ

- Central側のMTUは接続時のsnapshotです。backendにclient側のMTU変更callbackがないため、接続後の変化は追跡できません（`docs/DECISIONS.ja.md` Connection/GATT #23参照）。

## 期待されるSerial出力

```
Connected with MTU 185 (notification payload up to 182 bytes)
```
