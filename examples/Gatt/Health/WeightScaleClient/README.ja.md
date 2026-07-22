# WeightScaleClient

> English: [README.md](README.md)

Weight Scale Service（0x181D）へ接続し、Weight Scale FeatureをRead、Weight MeasurementのIndicationを購読して、0.005 kg分解能のuint16体重をデコードします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Weight Scale Peripheral: [WeightScaleServer](../WeightScaleServer/) example または市販の体重計

## 動作

- 0x181DをAdvertiseする機器をscanして接続
- Weight Scale Feature（0x2A9E）をRead
- Weight Measurement（0x2A9D）の**Indication**を購読（`subscribe(..., false)`）
- flags＋uint16体重（SI 0.005 kg/LSB、Imperial 0.01 lb/LSB）をデコードして表示

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic, false)` — Indicationを購読

## 期待されるSerial出力

```
Weight: 70.000 kg
Weight: 70.100 kg
```
