# BodyCompositionClient

> English: [README.md](README.md)

Body Composition Service（0x181B）へ接続し、Body Composition FeatureをRead、Body Composition MeasurementのIndicationを購読して、Body Fat Percentage（0.1 %/LSB）と任意のWeightフィールドをデコードします。

## ハードウェア

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
