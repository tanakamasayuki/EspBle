# CurrentTimeClient

> English: [README.md](README.md)

Current Time Service（0x1805）へ接続し、10バイトのCurrent Time（0x2A2B）をReadしてdecodeした後、Notificationを購読して各更新をdecodeします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Current Time Peripheral: [CurrentTimeServer](../CurrentTimeServer/) example

## 動作

- 0x1805をAdvertiseする機器をactive scanして接続
- Current Time（0x2A2B）をReadし、10バイトwire形式（year、日付、時刻、weekday、fraction、adjust reason）をdecode
- Current TimeのNotificationを購読し、各変更をdecode

## 主なAPI

- `ble.readCharacteristic(...)` — 初期のCurrent TimeをRead
- `ble.subscribe(...)` — Current TimeのNotificationを購読
- `ble.onNotification(...)` — Notifyされた各更新をdecode

## 期待されるSerial出力

```
Current Time: 2026-07-19 12:34:56 weekday=7 fraction=0/256 reason=0x01
Current Time subscription: ready
Current Time changed: 2026-07-19 12:34:57 weekday=7 fraction=0/256 reason=0x01
```
