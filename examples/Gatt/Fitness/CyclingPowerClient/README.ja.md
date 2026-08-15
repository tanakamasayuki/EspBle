# CyclingPowerClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Cycling Power Service（0x1818）のCentral / GATT Clientです。Sensor Location（0x2A5D）をReadし、Cycling Power Measurement（0x2A63）のNotificationを購読して、符号付き16bit instantaneous powerをデコードします。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- Peripheral × 1: [CyclingPowerServer](../CyclingPowerServer/) example

## 動作

- 0x1818をadvertiseする接続可能なpeerをscanして接続
- 接続時にSensor LocationをReadして表示
- Cycling Power Measurementを購読し、16bit flagsフィールドの後に続く符号付き16bit instantaneous power（ワット）をデコード

## 主なAPI

- `ble.readCharacteristic(...)` — Sensor Location を Read
- `ble.subscribe(...)` — Cycling Power Measurement の Notification を有効化
- `ble.onNotification(callback)` — power フィールドをデコード

## メモ

- powerフィールドは`static_cast<int16_t>`で解釈されるため、負のpower（惰走など）も正しくデコードされます。

## 期待されるSerial出力

```
Sensor location: 6
Power: 210 W
Power: 220 W
Power: 230 W
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
