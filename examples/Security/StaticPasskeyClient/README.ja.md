# StaticPasskeyClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 3章「セキュリティ編」

[StaticPasskeyServer](../StaticPasskeyServer/)のCentral側、MITM認証Pairingでpasskeyを「入力する」側（`KeyboardOnly`）です。Pairing成功後、認証済みlinkを要求するCharacteristicをDiscovery→Readします。

passkeyをsketchに固定するため、実行時の入力を伴いません。**利用者が実際に打ち込む形**は[RuntimePasskeyClient](../RuntimePasskeyClient/)を参照してください。

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

- 固定passkeyはスタックへ事前に渡されるため、ここの`STATIC_PASSKEY`はServerが表示する値と一致させる必要があります。**値がsketchに焼き込まれる以上、ソースを読める相手には秘密になりません。** 実運用では[RuntimePasskeyServer](../RuntimePasskeyServer/) / [RuntimePasskeyClient](../RuntimePasskeyClient/)の形を選んでください。
- `authenticatedRead`のCharacteristicはMITM Pairing完了後にのみReadできます。それ以前のReadはATTのsecurityエラーで失敗します。

## 期待されるSerial出力

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```

## 関連するガイド

- [BLE入門ガイド §3 セキュリティ編](../../../docs/GUIDE_BLE_BASICS.ja.md#3-セキュリティ編--つながった相手をどこまで信頼するか) — pairing・bond・IO capability

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[StaticPasskeyClient.ino](StaticPasskeyClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[StaticPasskeyServer](../StaticPasskeyServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

StaticPasskeyClient - StaticPasskeyServerのCentral側。passkeyを「入力する」側 （KeyboardOnly）。passkeyはsketchに固定して事前にスタックへ渡すため、実行時の入力は 伴わない。利用者が打ち込む形は Security/RuntimePasskeyClient を参照。 MITM認証Pairing完了後、保護されたCharacteristicをReadする。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.onSecurityChanged(...)` — セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
4. `ble.discoverCharacteristic(...)` — 対象のリモートCharacteristicを探し、対応する探索完了イベントから読み書きを続けます。
5. `ble.onCharacteristicDiscovered(...)` — 探索成功を確認してから進みます。接続しただけでは対象UUIDの存在まで保証されません。
6. `ble.readCharacteristic(...)` — 相手の値のReadをキューへ登録します。値は即時のboolではなくonCharacteristicReadへ届きます。
7. `ble.onCharacteristicRead(...)` — 成功・UUID・値の長さを確認してからデコードします。バイナリのStringには0バイトも含まれるので、長さを使って処理します。
8. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
9. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
10. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

固定パスキーはデモ用設定で、すべての配備済み機器に適した秘密ではありません。Serverの設定に合わせ、保護された値を使う前に認証状態を確認します。

Pairingはセキュリティを確立し、Bondingは次回のために信頼情報を保存します。リセットしても毎回確認が出るとは限りません。encrypted・authenticated・bondedを分けて確認し、利用者の回答待ちでもupdateを回します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
