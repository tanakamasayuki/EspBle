# StaticPasskeyClient

> English: [README.md](README.md)

[StaticPasskeyServer](../StaticPasskeyServer/)のCentral側、MITM認証Pairingでpasskeyを「入力する」側（`KeyboardOnly`）です。Pairing成功後、認証済みlinkを要求するCharacteristicをDiscovery→Readします。

## 必要なもの

- 1 × ESP32-S3（このsketch。Central、passkey入力側）
- 1 × ESP32-S3（[StaticPasskeyServer](../StaticPasskeyServer/) exampleを動かす）

## 動作

- ServerのService UUIDをactive scanし、最初の一致へ接続します
- 接続時に`requestSecurity()`で明示的にPairingを開始します
- Security成功時にMITM保護されたCharacteristicをDiscoveryしてReadします
- Securityの結果と保護された値を表示します
- Serialコマンド`c`で全Bondを削除し（切断中のみ許可）、残数を表示します

## 主なAPI

- `EspBleSecurityConfig::ioCapability = KeyboardOnly` — passkeyを「入力する」側
- `config.security.staticPasskeyEnabled` / `staticPasskey` — スタックへ渡す事前設定passkey
- `ble.requestSecurity(connectionId)` — 明示的なPairing開始。完了は`onSecurityChanged()`へ届きます
- `ble.discoverCharacteristic(...)` / `ble.readCharacteristic(...)` — Pairing後にCharacteristicへアクセス

## メモ

- 現在の試行APIは実行時入力を待つ代わりに事前設定passkeyをスタックへ渡すため、ここの`STATIC_PASSKEY`はServerが表示する値と一致させる必要があります。
- `authenticatedRead`のCharacteristicはMITM Pairing完了後にのみReadできます。それ以前のReadはATTのsecurityエラーで失敗します。

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```
