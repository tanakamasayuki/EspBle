# BatteryServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Battery Service（`0x180F`）と、Read・Notify可能なBattery Level Characteristic（`0x2A19`）を公開するPeripheralです。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [BatteryClient](../BatteryClient/) example、または任意のBLE Central/スマートフォン

## 動作

- `begin()`の前にBattery Level Characteristic（Read + Notify）を登録し、`0x180F`をAdvertise
- 初期値は75%で、Clientが通知をON/OFFするとログを出力
- Serialから`+` / `-`を送るとレベルを変更（0〜100にクランプ）。変更のたびに値を更新し購読者へNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .readable = true, .notifiable = true })` — Battery Level Characteristicを宣言
- `ble.gattServer().setValue(...)` — 現在のレベルを保存
- `ble.gattServer().notify(...)` — 購読者へレベルを送信（受理されたか返す）
- `ble.gattServer().onSubscriptionChanged(...)` — 通知の有効/無効を検知

## 期待されるSerial出力

```
Send '+' or '-' to change the Battery Level.
Battery notifications: 1
Battery: 76% (notification accepted: 1)
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
