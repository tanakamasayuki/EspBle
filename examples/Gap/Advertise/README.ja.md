# Advertise

> English: [README.md](README.md)

デバイス名と16-bit Service UUIDを載せたconnectableなLegacy Advertisingを開始します。スマートフォンの汎用BLEスキャナアプリ（nRF Connectなど）か、2台目のボードで[Scan](../Scan/) exampleを動かして確認できます。

## ハードウェア

- ESP32-S3 × 1（またはEspBle対応ボード）
- BLEスキャナ（スマートフォンアプリ、またはScan exampleを動かす2台目のボード）

## 動作内容

- デバイス名`EspBle Advertiser`でスタックを初期化します
- local nameとHID Service UUID `1812`を単一のComplete List AD構造としてadvertiseします
- リセットするまでadvertiseし続けます（Centralからの接続はスタックが受け入れます）

## 主なAPI

- `ble.begin(config)` — スタック初期化。`config.deviceName`がGAPデバイス名になります
- `ble.advertising().setName(name)` — advertising payloadへlocal nameを載せます
- `ble.advertising().addServiceUuid(uuid)` — 16/32/128-bit Service UUIDを掲載（サイズごとに単一のComplete Listへまとめます）
- `ble.advertising().start()` — connectableなLegacy Advertisingを開始。payloadが31 bytesを超える場合は`InvalidArgument`で失敗します
- `ble.lastErrorName()` / `ble.lastErrorDetail()` — 要求が拒否された理由

## 期待されるSerial出力

成功時は何も表示しません。失敗時:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```
