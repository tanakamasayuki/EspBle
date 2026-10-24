# JustWorksServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 3章「セキュリティ編」

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

## 関連するガイド

- [BLE入門ガイド §3 セキュリティ編](../../../docs/GUIDE_BLE_BASICS.ja.md#3-セキュリティ編--つながった相手をどこまで信頼するか) — pairing・bond・IO capability

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[JustWorksServer.ino](JustWorksServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

JustWorksServer - 暗号化されたlinkを要求するCharacteristicを持つGATT Server。 PairingはJust Works（passkeyなし、LE Secure Connections）+ Bondingで、接続時に自動開始する。 暗号化前にCharacteristicを読むとinsufficient-encryptionエラーになり、OS側のPairingが誘発される。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `gattServer.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `gattServer.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `gattServer.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `gattServer.onWritten(...)` — updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
5. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
6. `ble.onSecurityChanged(...)` — セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
7. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
8. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。
9. `ble.deleteAllBonds(...)` — 保存した信頼情報を意図して削除します。再起動だけでは消えません。再ペアリング時は相手側の対応する登録も削除します。

## 確認と変更のポイント

Just Worksではリンクを暗号化できても、中間者に対する認証はできません。パスキーの比較手順はないため、その性質が必要なら認証付きペアリング例を使います。

Pairingはセキュリティを確立し、Bondingは次回のために信頼情報を保存します。リセットしても毎回確認が出るとは限りません。encrypted・authenticated・bondedを分けて確認し、利用者の回答待ちでもupdateを回します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
