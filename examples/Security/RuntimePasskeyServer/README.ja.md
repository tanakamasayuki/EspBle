# RuntimePasskeyServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 3章「セキュリティ編」

Passkey Entryの**表示側**（`DisplayOnly`）です。passkeyをsketchに固定せず、**Pairingのたびにスタックが新しい6桁を生成**します。相手側は[RuntimePasskeyClient](../RuntimePasskeyClient/)です。

[StaticPasskeyServer](../StaticPasskeyServer/)との違いは1点だけ、`staticPasskeyEnabled` を立てないことです。それだけで値が毎回変わります。**固定passkeyはsketchに焼き込まれる以上、ソースを読める相手には秘密になりません。** 画面を持つ製品ではこちらが本来の形です。

## 必要なもの

- 1 × ESP32-S3（このsketch。Peripheral、表示側）
- 1 × ESP32-S3（[RuntimePasskeyClient](../RuntimePasskeyClient/) を動かす）

## 動作

- `authenticatedRead` を要求するCharacteristicを持つGATT Serverとしてadvertiseします
- Centralが接続するとPairingが始まり、スタックが生成した6桁が `onPasskeyDisplayed` に届きます
- Serialへ表示されたその値を、人間がClient側へ入力します（BLEの外を通す必要があります。これがMITM保護の根拠です）
- 結果は `onSecurityChanged` に届きます。成功時は `authenticated=1` です
- `c` で全Bondを削除します（切断中のみ）

## 主なAPI

- `EspBleSecurityConfig::ioCapability = DisplayOnly` — passkeyを表示する側
- `staticPasskeyEnabled` を**設定しない** — これがpasskeyを毎回生成させる指定
- `ble.onPasskeyDisplayed(cb)` — 表示すべき6桁が `event.passkey` で届く

## 注意

- **2回目以降は表示されません。** Bond済みの相手は保存鍵で暗号化するため、Pairingそのものが起きません。もう一度見るには**両側で**Bondを削除してください（`c`）。
- 表示された値は `update()` のcontextで届きます。SMPはこの間、答えを待って停止しています。

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Enter passkey 481907 on the peer.
Security established: encrypted=1 authenticated=1 bonded=1
```
