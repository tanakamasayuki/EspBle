# StaticPasskeyServer

> English: [README.md](README.md)

静的6桁passkeyによるMITM認証Pairingを要求するGATT Server（Peripheral）です。このボードは表示側（`DisplayOnly`）で、passkeyをSerialへ表示し、接続するCentralがそれを入力します。[StaticPasskeyClient](../StaticPasskeyClient/) example（passkey入力側）と接続します。

## 必要なもの

- 1 × ESP32-S3（このsketch。Peripheral / GATT Server、passkey表示側）
- 1 × キー入力できるBLE Central: [StaticPasskeyClient](../StaticPasskeyClient/) exampleまたはスマートフォンアプリ

## 動作

- `begin()`前に`authenticatedRead` / `authenticatedWrite` permissionつきのCharacteristicを登録します
- `mitm`、`ioCapability = DisplayOnly`、静的passkey `438209`でsecurityを有効化します
- Pairing開始時に`onPasskeyDisplayed`でpasskeyを表示します
- `onSecurityChanged`でSecurityの結果を表示します（成功時は`authenticated=1`）
- 切断のたびにadvertisingを再開します

## 主なAPI

- `EspBleGattCharacteristicConfig::authenticatedRead` / `authenticatedWrite` — MITM認証済みlinkを要求します
- `EspBleSecurityConfig` — `mitm`、`ioCapability`（`DisplayOnly` / `KeyboardOnly`）、`staticPasskeyEnabled`、`staticPasskey`
- `ble.onPasskeyDisplayed(callback)` — `ble.update()`から配送。ユーザーへ提示するpasskeyを受け取ります
- `ble.onSecurityChanged(callback)` — `event.connection.authenticated`を確認します

## メモ

- CharacteristicはMITM認証済みlinkを要求するため、Just Works Pairingではアクセスできません。
- passkeyはexample用の固定値です。製品では共通のhard-coded値ではなく、デバイスごとに安全にprovisioningしてください。

## 期待されるSerial出力

```
Enter passkey 438209 on the peer.
Security established: encrypted=1 authenticated=1 bonded=1
```
