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

## 関連するガイド

- [BLE入門ガイド §3 セキュリティ編](../../../docs/GUIDE_BLE_BASICS.ja.md#3-セキュリティ編--つながった相手をどこまで信頼するか) — pairing・bond・IO capability

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[RuntimePasskeyServer.ino](RuntimePasskeyServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[RuntimePasskeyClient](../RuntimePasskeyClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

RuntimePasskeyServer - Passkey Entryの表示側。passkeyをsketchに固定せず、 Pairingごとに生成する。静的passkeyを設定しなければスタックが6桁を新たに引き、 onPasskeyDisplayed へ渡す。実機なら画面に表示する場面。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `gattServer.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `gattServer.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `gattServer.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
5. `ble.onSecurityChanged(...)` — セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
6. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
7. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。
8. `ble.deleteAllBonds(...)` — 保存した信頼情報を意図して削除します。再起動だけでは消えません。再ペアリング時は相手側の対応する登録も削除します。

## 確認と変更のポイント

今回のペアリング用パスキーを表示し、Client側で入力します。表示イベントはペアリング完了ではありません。認証済みアクセスと判断する前にonSecurityChangedを確認します。

Pairingはセキュリティを確立し、Bondingは次回のために信頼情報を保存します。リセットしても毎回確認が出るとは限りません。encrypted・authenticated・bondedを分けて確認し、利用者の回答待ちでもupdateを回します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
