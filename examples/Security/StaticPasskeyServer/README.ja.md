# StaticPasskeyServer

> English: [README.md](README.md)

静的6桁passkeyによるMITM認証Pairingを要求するGATT Serverです。ボードは表示側（`DisplayOnly`）として動作し、passkeyをSerialへ表示します。接続するCentral側がそのpasskeyを入力します。CharacteristicはMITM認証済みlinkを要求するため、Just Works Pairingではアクセスできません。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server、passkey表示側）
- キー入力できるBLE Central × 1（スマートフォンアプリまたは2台目のボード）

## 動作内容

- `begin()`前に`authenticatedRead` / `authenticatedWrite` permissionつきのCharacteristicを登録します
- `mitm`、`ioCapability = DisplayOnly`、静的passkey `438209`でsecurityを有効化します
- Pairing開始時にpasskeyを表示し、完了時にSecurityの結果を表示します（成功時は`authenticated=1`）
- 切断のたびにadvertisingを再開します

passkeyはexample用の固定値です。製品では共通のhard-coded値ではなく、デバイスごとに安全にprovisioningしてください。

## 主なAPI

- `EspBleGattCharacteristicConfig::authenticatedRead` / `authenticatedWrite` — MITM認証済みlinkを要求します
- `EspBleSecurityConfig` — `mitm`、`ioCapability`（`DisplayOnly` / `KeyboardOnly`）、`staticPasskeyEnabled`、`staticPasskey`
- `ble.onPasskeyDisplayed(callback)` — `ble.update()`から配送されます。ユーザーへ提示するpasskeyを受け取ります
- `ble.onSecurityChanged(callback)` — `event.connection.authenticated`を確認します

## 期待されるSerial出力

```
Enter passkey 438209 on the peer.
Security established: encrypted=1 authenticated=1 bonded=1
```
