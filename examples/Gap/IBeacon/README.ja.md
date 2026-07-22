# IBeacon

> English: [README.md](README.md)

Apple iBeaconをbroadcastします。non-connectable・non-scannableなadvertisementのmanufacturer dataに、proximity UUID・major・minor・measured powerを載せます。レイアウトはbackend非依存の`EspBleIBeacon.h` codecで組み立てます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（broadcaster）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect / Locate Beacon等のbeaconアプリ

## 動作

- `espBleEncodeIBeacon(...)`で25 byteのiBeacon manufacturer data（Apple company ID `0x004C`、type `0x02`、length `0x15`、UUID、major、minor、measured power）を構築します
- 設定した間隔で純粋なnon-scannable advertisementとしてbroadcastします

## 主なAPI

- `EspBleIBeaconData` — proximity `uuid[16]`、`major`、`minor`、`measuredPower`
- `espBleEncodeIBeacon(beacon, out)` — `EspBleIBeaconManufacturerDataSize`（25）byteへencode
- `espBleDecodeIBeacon(manufacturerData, length, beacon)` / `espBleIsIBeacon(...)` — スキャナ側でdecode/検証
- `ble.advertising().setConnectable(false)` / `setScanResponseEnabled(false)` / `setManufacturerData(...)` / `setInterval(...)`

## メモ

- `measuredPower`は1 m時の校正RSSI（通常は負値、例: -59）で、受信側が距離推定に使います。
- UUID / major / minorは自分の値に置き換えてください。

## 期待されるSerial出力

成功時は何も表示しません。失敗時:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```
