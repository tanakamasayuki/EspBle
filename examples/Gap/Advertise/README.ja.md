# Advertise

> English: [README.md](README.md)

デバイス名と16-bit Service UUID（HID、`1812`）を載せたconnectableなLegacy Advertisingを開始します。Peripheral側の最小例です。汎用BLEスキャナアプリか、2台目のボードで組み合わせる[Scan](../Scan/) exampleで確認できます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect等のスキャナアプリ

## 動作

- デバイス名`EspBle Advertiser`でスタックを初期化します
- local nameとHID Service UUID `1812`を、サイズごとに単一のComplete List AD構造へまとめてadvertiseします（CSS Part A 1.1）
- リセットするまでadvertiseし続けます。Centralからの接続要求はスタックが受け入れます

## 主なAPI

- `ble.begin(config)` — スタック初期化。`config.deviceName`がGAPデバイス名になります
- `ble.advertising().setName(name)` — advertising payloadへlocal nameを載せます
- `ble.advertising().addServiceUuid(uuid)` — Service UUIDを掲載（サイズごとに単一のComplete Listへまとめます）
- `ble.advertising().start()` — connectableなLegacy Advertisingを開始。payloadが31 bytesを超える場合は`InvalidArgument`で失敗します
- `ble.lastErrorName()` / `ble.lastErrorDetail()` — 要求が拒否された理由

## 期待されるSerial出力

成功時は何も表示しません。失敗時:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```
