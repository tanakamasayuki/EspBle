# AlertNotificationClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Alert Notification Service（0x1811）へ接続し、Supported New Alert CategoryをRead、New Alertを購読、Control Pointへ「Notify New Alert Immediately」コマンドをWriteして、発火したNew Alert（category、count、text）をデコードします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Alert Notification Peripheral: [AlertNotificationServer](../AlertNotificationServer/) example

## 動作

- 0x1811をAdvertiseする機器をscanして接続
- Supported New Alert Category（0x2A47）をReadしbitmaskを表示
- New Alert（0x2A46）の**Notification**を購読
- Alert Notification Control Point（0x2A44）へ、Email（category 1）に対する「Notify New Alert Immediately」（command 2）をWrite
- 発火したNew Alert（Category ID＋count＋text）をデコード

## 主なAPI

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — 応答ありのControl Point書き込み

## 期待されるSerial出力

```
Supported categories: 0x0022
New Alert: category 1, count 3, text "Bob"
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
