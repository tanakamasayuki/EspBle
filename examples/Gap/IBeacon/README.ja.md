# IBeacon

> English: [README.md](README.md)

**Apple iBeacon**をbroadcastします。non-connectable・non-scannableなadvertisementのmanufacturer dataに、proximity UUID・major・minor・measured powerを載せます。レイアウトはbackend非依存の`EspBleIBeacon.h` codecで組み立てます。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（beacon）
- BLE scanner × 1（[Gap/Scan](../Scan/) example、またはnRF Connect / Locate Beacon等のbeaconアプリ）

## 動作内容

- `espBleEncodeIBeacon(...)`で25 byteのiBeacon manufacturer data（Apple company ID `0x004C`、type `0x02`、length `0x15`、UUID、major、minor、measured power）を構築
- `setConnectable(false)` ＋ `setScanResponseEnabled(false)` — 純粋なnon-scannable broadcaster
- `setManufacturerData(...)` — iBeacon payloadを送信
- `setInterval(100, 150)` — 送信間隔（ミリ秒）

## 主なAPI

- `EspBleIBeaconData` — proximity `uuid[16]`、`major`、`minor`、`measuredPower`
- `espBleEncodeIBeacon(beacon, out)` — `EspBleIBeaconManufacturerDataSize`（25）byteへencode
- `espBleDecodeIBeacon(manufacturerData, length, beacon)` / `espBleIsIBeacon(...)` — scanner側でdecode/検証

## メモ

- `measuredPower`は1 m時の校正RSSI（通常は負値、例: -59）で、受信側が距離推定に使います。
- UUID / major / minorは自分の値に置き換えてください。
