# JustWorksServer

> English: [README.md](README.md)

暗号化されたlinkを要求するCharacteristicを持つGATT Serverです。PairingはJust Works（passkeyなし、LE Secure Connections）+ Bondingで、接続時に自動開始します。スマートフォン（nRF Connectなど）や別ボードから接続してください。暗号化前にCharacteristicを読むとinsufficient-encryptionエラーになり、OS側のPairingが誘発されます。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- Pairing可能なBLE Central × 1（スマートフォンアプリまたは2台目のボード）

## 動作内容

- `begin()`前に`encryptedRead` / `encryptedWrite` permissionつきのCharacteristicを登録します
- Bondingと`pairOnConnect`つきでsecurityを有効化します（Centralの接続と同時にPairingを開始）
- Securityの結果（encrypted、authenticated=Just Worksではfalse、bonded、鍵長）を表示します
- 切断のたびにadvertisingを再開します
- Serialで`c`を受けると全Bondを削除します（切断中のみ許可）

## Serialコマンド

| コマンド | 動作 |
|---------|------|
| `c` | 全Bondを削除して残数を表示 |

## 主なAPI

- `EspBleGattCharacteristicConfig::encryptedRead` / `encryptedWrite` — ATT層でSecurity Mode 1 Level 2を強制します
- `EspBleSecurityConfig` — `enabled`、`bonding`、`pairOnConnect`
- `ble.onSecurityChanged(callback)` — `event.success`とConnectionのsecurity snapshot
- `ble.deleteAllBonds()` / `ble.bondCount()` — Bond storeの管理

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=0 bonded=1 keySize=16
Encrypted write: ...
```
