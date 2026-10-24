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

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[NumericComparisonServer.ino](NumericComparisonServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[NumericComparisonClient](../NumericComparisonClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

NumericComparisonServer - Numeric Comparison PairingのPeripheral側。 両側が DisplayYesNo かつMITM要求とすると、LE Secure Connectionsが同じ6桁を 両方に表示し、それぞれの利用者が一致を確認する。入力は不要で、値は比較するだけ。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `gattServer.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `gattServer.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `gattServer.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
5. `ble.onNumericComparison(...)` — 照合用の数値を表示し、利用者の明示的な判断を受け取ります。シリアル入力を待つ間もupdateを回します。
6. `ble.onSecurityChanged(...)` — セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
7. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
8. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。
9. `ble.confirmNumericComparison(...)` — 両側の表示値を比較してから承認します。不一致は拒否し、イベントを自動承認しないようにします。
10. `ble.deleteAllBonds(...)` — 保存した信頼情報を意図して削除します。再起動だけでは消えません。再ペアリング時は相手側の対応する登録も削除します。

## 確認と変更のポイント

相手にも同じ数値が表示され、利用者が一致を確認してから承認します。最初から試す場合は切断中に双方の対応するbondを削除します。

Pairingはセキュリティを確立し、Bondingは次回のために信頼情報を保存します。リセットしても毎回確認が出るとは限りません。encrypted・authenticated・bondedを分けて確認し、利用者の回答待ちでもupdateを回します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
