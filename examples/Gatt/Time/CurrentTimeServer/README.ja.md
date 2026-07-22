# CurrentTimeServer

> English: [README.md](README.md)

標準Current Time Service（0x1805）のPeripheral。Current Time（0x2A2B）はread/**notify**のCharacteristicで、10バイトの標準wire形式（year LE、month、day、hours、minutes、seconds、weekday、fraction256、adjust reason）を保持します。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [CurrentTimeClient](../CurrentTimeClient/) example、または Current Time client

## 動作

- `begin()`の前にCurrent Timeを登録し、0x1805をAdvertise
- 固定のデモ時刻（2026-07-19 12:34:56、日曜、手動調整）で値を初期化
- Serialから`t`を送るとsecondsフィールドを1進め、値を設定して購読中のClientへNotify

## 主なAPI

- `ble.gattServer().setValue(...)` — 保持するCurrent Time値を更新
- `ble.gattServer().notify(...)` — 新しい時刻を購読者へNotify

## 期待されるSerial出力

```
Send 't' to advance one second and notify subscribers.
Time: 2026-07-19 12:34:57 (notification accepted: 1)
```
