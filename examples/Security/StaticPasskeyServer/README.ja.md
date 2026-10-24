# StaticPasskeyServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 3章「セキュリティ編」

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

## 関連するガイド

- [BLE入門ガイド §3 セキュリティ編](../../../docs/GUIDE_BLE_BASICS.ja.md#3-セキュリティ編--つながった相手をどこまで信頼するか) — pairing・bond・IO capability

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[StaticPasskeyServer.ino](StaticPasskeyServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[StaticPasskeyClient](../StaticPasskeyClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

StaticPasskeyServer - 静的6桁passkeyによるMITM認証Pairingを要求するGATT Server。 ボードは表示側（DisplayOnly）で、passkeyをSerialへ表示する。接続するCentralがそれを入力する。 CharacteristicはMITM認証済みlinkを要求するため、Just Works Pairingではアクセスできない。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `gattServer.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `gattServer.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `gattServer.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
5. `ble.onSecurityChanged(...)` — セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
6. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
7. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

Static PasskeyはClientと一致させ、ペアリングごとには変わりません。例の値を共通の製品用秘密として配るのではなく、用途に合う初期設定／認証方針を決めます。

Pairingはセキュリティを確立し、Bondingは次回のために信頼情報を保存します。リセットしても毎回確認が出るとは限りません。encrypted・authenticated・bondedを分けて確認し、利用者の回答待ちでもupdateを回します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
