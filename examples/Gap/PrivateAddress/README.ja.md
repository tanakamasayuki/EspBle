# PrivateAddress

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

`EspBleConfig::ownAddressType`で選択し、工場出荷のpublic addressの代わりにprivate addressでadvertiseします。connectableなPeripheralの例です。2台目のボードで組み合わせる[Scan](../Scan/) exampleでaddress typeを確認できます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect等のスキャナアプリ

## 2つのモード

sketch冒頭の `USE_RESOLVABLE_PRIVATE_ADDRESS` で切り替えます。

| | RandomStatic（既定） | ResolvablePrivate（RPA） |
|---|---|---|
| アドレス | `begin()`で生成する固定random | controllerが周期的に変える |
| 追跡耐性 | public addressは隠せるが、この固定値で追跡はできる | 回転するので追跡されにくい |
| bonding | 不要 | **必須**。peerはIRKでアドレスを解決する |
| 単体で動くか | 動く | securityなしでは相手が再接続できない |

RPAの回転周期は同梱NimBLEの `CONFIG_BT_NIMBLE_RPA_TIMEOUT`（900秒）で固定されており、アプリからは変更できません。

## 動作

- 選んだモードに応じて `config.ownAddressType` を設定します。RPAモードでは `config.security.enabled` / `bonding` も併せて有効にします
- connectableなPeripheralとしてadvertiseし、スキャナがaddress typeを観測できるようにします
- 接続時に相手のアドレスとbonded状態を表示します

## 主なAPI

- `EspBleConfig::ownAddressType` — `Public`（既定） / `RandomStatic` / `ResolvablePrivate`
- `EspBleConfig::security.enabled` / `bonding` — RPAを使う場合に必要
- `EspBleConnection::peerAddress` / `bonded` — 相手側から見えたアドレスとbonding状態

## メモ

- `Public` — 工場出荷のpublic address。`RandomStatic` — `begin()`で生成する固定random static address（回転しない固定identity）。`ResolvablePrivate` — controllerが周期回転させるRPA（`CONFIG_BT_NIMBLE_RPA_TIMEOUT`、同梱ビルドで900秒）。bonded peerがIRKで解決するためsecurity/bonding併用時のみ有用で、未bondのスキャナには変化するrandom addressにしか見えません。
- スキャナからはaddress typeが**Random**（Publicではない）に見えます。static random addressは先頭octetの上位2bitが`0b11`です。
- Extended / Periodic Advertisingは同梱NimBLEが`CONFIG_BT_NIMBLE_EXT_ADV`無効でビルドされているため利用できません。

- accept listはアドレスで照合するため、RPAを使う相手はbondingしてidentity addressが効くようになるまで登録できません（[AcceptList](../AcceptList/)参照）。

## 期待されるSerial出力

```
Advertising with a random static address.
Connected id=1 peer=d0:cf:13:58:fd:95 bonded=0
Disconnected id=1
```
