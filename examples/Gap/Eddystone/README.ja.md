# Eddystone

> English: [README.md](README.md)

**Eddystone-URL** beaconをbroadcastします。non-connectable・non-scannableなadvertisementのService Data（UUID `0xFEAA`）に圧縮したURLを載せます。frameはbackend非依存の`EspBleEddystone.h` codecで組み立てます。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（beacon）
- BLE scanner × 1（[Gap/Scan](../Scan/) example、またはnRF Connect / Physical Web等のEddystone対応アプリ）

## 動作内容

- `espBleEncodeEddystoneUrl(...)`でURL frame（frame type `0x10`、TX power、schemeバイト、ドメインサフィックス圧縮URL）を構築
- `addServiceUuid("FEAA")` — scannerが発見できるようEddystone UUIDを一覧に載せる
- `setServiceData("FEAA", ...)` — URL frameをService Dataとして送信
- `setConnectable(false)` ＋ `setScanResponseEnabled(false)` — 純粋なnon-scannable broadcaster

## 主なAPI

- `espBleEncodeEddystoneUrl(url, txPower, out, outCapacity, outLength)` — URL frame bodyをencode
- `espBleDecodeEddystoneUrl(serviceData, length, urlOut, urlCapacity, txPower)` / `espBleIsEddystoneUrl(...)` — scanner側でdecode/検証（`EspBleScanResult::serviceData`から）
- `EspBleAdvertising::setServiceData(uuid, data, length)` — Service Dataブロックを付与

## メモ

- URL schemeは`http://` / `https://` / `http://www.` / `https://www.`のいずれか。よく使うサフィックス（`.com/`、`.org`など）は1バイトへ圧縮されます。legacy advertising budgetに収まらないURLはencoderが拒否します。
- `txPower`は0 m時の校正RSSI（dBm）で、受信側が距離推定に使います。
- URL frameのみ対応です（UID / TLM / EIDは未実装）。
