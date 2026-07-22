# EnvironmentalServer

> English: [README.md](README.md)

標準Environmental Sensing Service（0x181A）のPeripheralです。Temperature（0x2A6E）、Humidity（0x2A6F）、Pressure（0x2A6D）を仕様のlittle-endian整数スケール（0.01 ℃、0.01 %、0.1 Pa）で公開します。TemperatureはRead / Notify可能、humidityとpressureはRead専用です。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [EnvironmentalClient](../EnvironmentalClient/) example、または任意のBLE Central/スマートフォン

## 動作

- Environmental Sensing serviceを登録し、0x181Aをadvertise
- Temperature 21.50 ℃、Humidity 48.75 %、Pressure 101325.0 Pa をReadable値として設定
- Serialを読み取り、`+` / `-` を送ると温度を0.25 ℃変更して再設定し、新しい値をNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .readable = true, .notifiable = true })` — Temperature
- `ble.gattServer().setValue(...)` — 保持しているcharacteristic値を更新
- `ble.gattServer().notify(...)` — 購読者へNotify。戻り値で受理されたかを確認できる

## メモ

- `notify()`は、まだ誰もTemperatureを購読していないとき0を、購読者が接続済みのとき1を返します。

## 期待されるSerial出力

```
Send '+' or '-' to change temperature by 0.25 C.
Temperature raw: 2175 (notification accepted: 1)
Temperature raw: 2200 (notification accepted: 1)
```
