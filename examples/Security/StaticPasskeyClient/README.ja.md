# StaticPasskeyClient

> English: [README.md](README.md)

[StaticPasskeyServer](../StaticPasskeyServer/)のCentral側、つまりMITM認証Pairingでpasskeyを「入力する」側（`KeyboardOnly`）です。現在の試行APIは実行時入力を待つ代わりに事前設定したpasskeyをスタックへ渡すため、Serverが表示する値と一致させておく必要があります。Pairing成功後、認証済みlinkを要求するCharacteristicをReadします。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Central、passkey入力側）
- [StaticPasskeyServer](../StaticPasskeyServer/) exampleを動かすESP32-S3 × 1

## 動作内容

- ServerのService UUIDをscanして接続します
- 接続時に`requestSecurity()`で明示的にPairingを開始します
- Securityの結果（成功時`authenticated=1`）を表示し、MITM保護されたCharacteristicをDiscovery→Readします
- `c`で全Bondを削除します（切断中のみ）

## Serialコマンド

| コマンド | 動作 |
|---------|------|
| `c` | 全Bondを削除して残数を表示 |

## 主なAPI

- `EspBleSecurityConfig::ioCapability = KeyboardOnly` — passkeyを「入力する」側
- `config.security.staticPasskeyEnabled` / `staticPasskey` — 事前設定passkey（実行時のpasskey入力は未実装）
- `ble.requestSecurity(connectionId)` — 明示的なPairing開始。完了は`onSecurityChanged()`へ届きます
- `authenticatedRead`のCharacteristicはMITM Pairing完了後にのみReadできます（それ以前のReadはATTのsecurityエラーで失敗します）

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```
