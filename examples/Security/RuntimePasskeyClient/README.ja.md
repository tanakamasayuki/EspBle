# RuntimePasskeyClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 3章「セキュリティ編」

Passkey Entryの**入力側**（`KeyboardOnly`）です。相手が表示した6桁を**実行時に受け取って** `providePasskey()` で渡します。相手側は[RuntimePasskeyServer](../RuntimePasskeyServer/)です。

[StaticPasskeyClient](../StaticPasskeyClient/)はpasskeyをsketchに固定しますが、こちらは実際の機器と同じく「利用者が見て打ち込む」流れです。

## 必要なもの

- 1 × ESP32-S3（このsketch。Central、入力側）
- 1 × ESP32-S3（[RuntimePasskeyServer](../RuntimePasskeyServer/) を動かす）

## 動作

- ServerのService UUIDをactive scanし、最初の一致へ接続します
- `pairOnConnect`（既定で有効）により、接続と同時にPairingが始まります
- スタックがpasskeyを要求し、**答えが来るまでPairingは止まります**
- Serialへ `p` に続けて6桁を送る（例: `p481907`）と `providePasskey()` が呼ばれ、Pairingが再開します
- 成功後、`authenticatedRead` を要求するCharacteristicをDiscovery→Readします
- `c` で全Bondを削除します（切断中のみ）

## 主なAPI

- `EspBleSecurityConfig::ioCapability = KeyboardOnly` — passkeyを入力する側
- `ble.providePasskey(passkey)` — 待機中のPairingへ6桁を渡す
- `ble.discoverCharacteristic(...)` / `ble.readCharacteristic(...)` — Pairing後のアクセス

## 注意

- **答えは30秒以内に返してください。** 超えるとスタックが待機を打ち切り、Pairingは失敗します。`loop()` を長時間ブロックする処理と併用しないでください。
- `providePasskey()` は待機の**前後どちらに呼んでも**受け付けられます。先に呼んだ値は次の要求で使われます。
- 値が違うとPairingはATT/SMPのエラーで失敗し、`onSecurityChanged` に `success=0` が届きます。**間違いを個別に知らせる仕組みはありません**（それを許すとpasskeyを1桁ずつ試せてしまうためです）。
- 2回目以降はBondが効いてPairing自体が起きないため、入力を求められません。試し直すには両側で `c` を送ります。

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Connected id=1. Type p<passkey> (e.g. p123456) shown on the peer.
Passkey 481907 provided
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```
