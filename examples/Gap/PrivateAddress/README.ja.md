# PrivateAddress

> English: [README.md](README.md)

`EspBleConfig::ownAddressType`を使い、工場出荷のpublic addressの代わりに**private address**でadvertiseします。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Peripheral）
- BLE scanner × 1（2枚目のboardで[Gap/Scan](../Scan/) example、またはnRF Connect等のスキャナアプリ）

## 動作内容

- `config.ownAddressType = EspBleOwnAddressType::RandomStatic` — public addressを隠す固定random static addressを提示
- connectableなPeripheralとしてadvertiseし、scannerがaddress typeを観測できるようにする

## address type

- `Public`（既定） — 工場出荷のpublic address
- `RandomStatic` — `begin()`で生成する固定random static address（回転しない固定random identity）
- `ResolvablePrivate` — controllerが周期回転するRPA（`CONFIG_BT_NIMBLE_RPA_TIMEOUT`、同梱ビルドで900秒）。bonded peerがbonding時のIRKで解決するためsecurity/bonding併用時のみ有用。未bondのscannerには変化するrandom addressにしか見えない

## メモ

- scannerからはaddress typeが**Random**（Publicではない）に見えます。static random addressは先頭octetの上位2bitが`0b11`です。
- Extended / Periodic Advertisingは同梱NimBLEが`CONFIG_BT_NIMBLE_EXT_ADV`無効でビルドされているため利用できません。
