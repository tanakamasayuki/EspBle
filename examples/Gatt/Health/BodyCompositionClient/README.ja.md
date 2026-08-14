# BodyCompositionClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Body Composition Service（0x181B）へ接続し、Body Composition FeatureをRead、Body Composition MeasurementのIndicationを購読して、Body Fat Percentage（0.1 %/LSB）と任意のWeightフィールドをデコードします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Body Composition Peripheral: [BodyCompositionServer](../BodyCompositionServer/) example または市販の体組成計

## 動作

- 0x181BをAdvertiseする機器をscanして接続
- Body Composition Feature（0x2A9B）をRead
- Body Composition Measurement（0x2A9C）の**Indication**を購読（`subscribe(..., false)`）
- flags＋Body Fat Percentageと、flag bit 10がセットされていれば任意のWeight（SI 0.005 kg/LSB、Imperial 0.01 lb/LSB）をデコードして表示

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic, false)` — Indicationを購読

## 期待されるSerial出力

```
Body fat: 27.5 %, weight: 70.000 kg
Body fat: 27.6 %, weight: 70.000 kg
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
