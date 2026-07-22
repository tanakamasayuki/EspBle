# ReferenceTimeUpdateClient

> English: [README.md](README.md)

Reference Time Update Service（0x1806）へ接続し、Time Update StateをRead、Time Update Control Point（**Write Without Response**）でreference update要求→キャンセルを行い、毎回stateを再readします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Reference Time Update Peripheral: [ReferenceTimeUpdateServer](../ReferenceTimeUpdateServer/) example

## 動作

- 0x1806をAdvertiseする機器をscanして接続
- 初期のTime Update State（0x2A17）をRead
- 2秒ごとに: Get Reference Update（Control Point 1）→ State read → Cancel Reference Update（Control Point 2）→ State read
- 各Current State＋Resultの組を表示

## 主なAPI

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, false)` — Write Without Response

## 期待されるSerial出力

```
Time Update State: current=0 result=0
Time Update State: current=1 result=0
Time Update State: current=0 result=1
```
