# NumericComparisonServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 3章「セキュリティ編」

**Numeric Comparison** PairingのPeripheral側です。相手側は[NumericComparisonClient](../NumericComparisonClient/)で、**両方が同じ設定**（`DisplayYesNo` ＋ MITM要求）である必要があります。

Passkey Entryとの違いは、**どちらも入力しない**ことです。LE Secure Connectionsが同じ6桁を両方に表示し、利用者は「両画面の数字が一致しているか」だけを答えます。画面はあるがキーボードが無い機器（スマートフォンとイヤホンなど）で使われる方式です。

## 必要なもの

- 1 × ESP32-S3（このsketch。Peripheral）
- 1 × ESP32-S3（[NumericComparisonClient](../NumericComparisonClient/) を動かす）

両方のSerialモニタを同時に見られるようにしてください。**2つの数字を見比べるのがこの方式の要点**です。

## 動作

- `authenticatedRead` を要求するCharacteristicを持つGATT Serverとしてadvertiseします
- Pairingが始まると、比較すべき6桁が `onNumericComparison` に届きます
- `y` で一致を承認、`n` で拒否します。**答えるまでPairingは止まっています**
- 両側が承認したときだけ暗号化が成立し、`onSecurityChanged` に `authenticated=1` が届きます
- `c` で全Bondを削除します（切断中のみ）

## 主なAPI

- `EspBleSecurityConfig::ioCapability = DisplayYesNo` — 表示とYes/Noができる
- `ble.onNumericComparison(cb)` — 比較する6桁が `event.passkey` で届く
- `ble.confirmNumericComparison(accept)` — 一致したかを答える

## 注意

- **両側がDisplayYesNoかつMITM要求でなければNumeric Comparisonは選ばれません。** 片方が `DisplayOnly` ならPasskey Entry、片方が `None` ならJust Worksになります。方式を直接指定するAPIはBLEに存在しません。
- **LE Secure Connectionsが前提です。** BLE 4.2より古い相手とは成立しません。
- **答えは30秒以内に返してください。** 超えるとスタックが待機を打ち切り、Pairingは失敗します。
- 数字が一致しないときは必ず `n` を送ってください。一致しないことがMITMの兆候そのものです。

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Does the peer show 052913? Send 'y' to accept, 'n' to reject.
Answer accept: sent
Security established: encrypted=1 authenticated=1 bonded=1
```

## 関連するガイド

- [BLE入門ガイド §3 セキュリティ編](../../../docs/GUIDE_BLE_BASICS.ja.md#3-セキュリティ編--つながった相手をどこまで信頼するか) — pairing・bond・IO capability
