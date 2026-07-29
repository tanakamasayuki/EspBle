# NumericComparisonClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 3章「セキュリティ編」

**Numeric Comparison** PairingのCentral側です。相手側は[NumericComparisonServer](../NumericComparisonServer/)で、設定は**Server側とまったく同じ**（`DisplayYesNo` ＋ MITM要求）です。両側が同じ申告をすることが、この方式が選ばれる条件そのものです。

## 必要なもの

- 1 × ESP32-S3（このsketch。Central）
- 1 × ESP32-S3（[NumericComparisonServer](../NumericComparisonServer/) を動かす）

両方のSerialモニタを同時に見られるようにしてください。

## 動作

- ServerのService UUIDをactive scanし、最初の一致へ接続します
- `pairOnConnect`（既定で有効）により、接続と同時にPairingが始まります
- 比較すべき6桁が `onNumericComparison` に届きます。Server側の表示と同じ値のはずです
- `y` で承認、`n` で拒否します。**答えるまでPairingは止まっています**
- 両側が承認したら、`authenticatedRead` を要求するCharacteristicをDiscovery→Readします
- `c` で全Bondを削除します（切断中のみ）

## 主なAPI

- `EspBleSecurityConfig::ioCapability = DisplayYesNo`
- `ble.onNumericComparison(cb)` / `ble.confirmNumericComparison(accept)`
- `ble.discoverCharacteristic(...)` / `ble.readCharacteristic(...)` — Pairing後のアクセス

## 注意

- **片側だけ拒否してもPairingは失敗します。** どちらか一方の `n` で全体が終わります。
- **答えは30秒以内に返してください。** 超えるとスタックが待機を打ち切ります。
- 2回目以降はBondが効いてPairing自体が起きないため、確認を求められません。試し直すには両側で `c` を送ります。

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Does the peer show 052913? Send 'y' to accept, 'n' to reject.
Answer accept: sent
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```
