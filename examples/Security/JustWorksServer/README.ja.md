# JustWorksServer

> English: [README.md](README.md)

暗号化されたlinkを要求するCharacteristicを持つGATT Server（Peripheral）です。PairingはJust Works（passkeyなし、LE Secure Connections）+ Bondingで、接続時に自動開始します。任意のBLE Central（nRF Connectなどのスマートフォンアプリ、または別ボード）と接続できます。

## 必要なもの

- 1 × ESP32-S3（このsketch。Peripheral / GATT Server）
- 1 × Pairing可能なBLE Central（スマートフォンアプリまたは2台目のボード）

## 動作

- `begin()`前に`encryptedRead` / `encryptedWrite` permissionつきのCharacteristicを登録します
- Bondingと`pairOnConnect`つきでsecurityを有効化し、Central接続と同時にPairingを開始します
- `onSecurityChanged`でSecurityの結果（encrypted、authenticated=Just Worksでは0、bonded、鍵長）を表示します
- 暗号化Characteristicへ書き込まれた値を表示します
- 切断のたびにadvertisingを再開します
- Serialコマンド`c`で全Bondを削除し（切断中のみ許可）、残数を表示します

## 主なAPI

- `EspBleGattCharacteristicConfig::encryptedRead` / `encryptedWrite` — ATT層で暗号化linkを強制します
- `EspBleSecurityConfig` — `enabled`、`bonding`、`pairOnConnect`
- `ble.onSecurityChanged(callback)` — `event.success`とConnectionのsecurity snapshot
- `ble.deleteAllBonds()` / `ble.bondCount()` — Bond storeの管理

## メモ

- Just Works Pairingは`encrypted=1`ですが`authenticated=0`（MITM非認証）です。暗号化前にCharacteristicを読むとinsufficient-encryptionエラーになり、OS側のPairingが誘発されます。

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=0 bonded=1 keySize=16
Encrypted write: hello
```
