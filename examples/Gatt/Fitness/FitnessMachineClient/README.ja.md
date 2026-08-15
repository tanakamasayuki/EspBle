# FitnessMachineClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Fitness Machine Service（0x1826）へ接続し、Fitness Machine Feature（0x2ACC）をRead、Indoor Bike Data（0x2AD2）を購読して、flagsに従うinstantaneous speed・cadence・powerをデコードします。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- Peripheral × 1: [FitnessMachineServer](../FitnessMachineServer/) example

## 動作

- Fitness Machine serviceのUUIDをscanして接続
- 8byteのFitness Machine FeatureをReadし、Indoor Bike Dataを購読
- flag順をたどってinstantaneous speed（0.01 km/h）・cadence（0.5/min→rpm）・符号付きpower（W）を特定してデコード

## 主なAPI

- `ble.readCharacteristic(...)` / `ble.subscribe(...)`
- `ble.onNotification(callback)` — Indoor Bike Data payloadをデコード

## 期待されるSerial出力

```
Fitness Machine Feature read; subscribing to Indoor Bike Data
Speed: 30.00 km/h  Cadence: 90 rpm  Power: 250 W
Speed: 31.00 km/h  Cadence: 90 rpm  Power: 250 W
...
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
