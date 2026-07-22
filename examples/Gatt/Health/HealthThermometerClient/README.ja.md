# HealthThermometerClient

> English: [README.md](README.md)

Health Thermometer Service（0x1809）へ接続し、Temperature TypeをRead、Temperature MeasurementのIndicationを購読して、IEEE-11073 32-bit FLOATの温度をデコードします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Health Thermometer Peripheral: [HealthThermometerServer](../HealthThermometerServer/) example または市販の体温計

## 動作

- 0x1809をAdvertiseする機器をscanして接続
- Temperature Type（0x2A1D）をRead
- Temperature Measurement（0x2A1C）の**Indication**を購読（`subscribe(..., false)`）
- 各測定値のflags＋IEEE-11073 FLOATをデコードして温度を表示

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic, false)` — Indicationを購読（Notificationではない）
- `espBleReadMedicalFloat32LE(bytes)` — IEEE-11073 32-bit FLOATをデコード（`EspBleMedicalFloat.h`）

## 期待されるSerial出力

```
Temperature type: 2
Temperature: 36.60 C
Temperature: 36.70 C
```
