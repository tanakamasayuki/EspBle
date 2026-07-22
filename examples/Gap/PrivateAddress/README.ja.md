# PrivateAddress

> English: [README.md](README.md)

`EspBleConfig::ownAddressType`で選択し、工場出荷のpublic addressの代わりにprivate addressでadvertiseします。connectableなPeripheralの例です。2台目のボードで組み合わせる[Scan](../Scan/) exampleでaddress typeを確認できます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect等のスキャナアプリ

## 動作

- `config.ownAddressType = EspBleOwnAddressType::RandomStatic` — public addressを隠す固定random static addressを提示します
- connectableなPeripheralとしてadvertiseし、スキャナがaddress typeを観測できるようにします

## 主なAPI

- `EspBleConfig::ownAddressType` — `Public`（既定） / `RandomStatic` / `ResolvablePrivate`
- `ble.advertising().setName(name)` / `ble.advertising().start()`

## メモ

- `Public` — 工場出荷のpublic address。`RandomStatic` — `begin()`で生成する固定random static address（回転しない固定identity）。`ResolvablePrivate` — controllerが周期回転させるRPA（`CONFIG_BT_NIMBLE_RPA_TIMEOUT`、同梱ビルドで900秒）。bonded peerがIRKで解決するためsecurity/bonding併用時のみ有用で、未bondのスキャナには変化するrandom addressにしか見えません。
- スキャナからはaddress typeが**Random**（Publicではない）に見えます。static random addressは先頭octetの上位2bitが`0b11`です。
- Extended / Periodic Advertisingは同梱NimBLEが`CONFIG_BT_NIMBLE_EXT_ADV`無効でビルドされているため利用できません。

## 期待されるSerial出力

成功時は何も表示しません。失敗時:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```
