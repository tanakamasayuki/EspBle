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

## 関連するガイド

- [BLE入門ガイド §3 セキュリティ編](../../../docs/GUIDE_BLE_BASICS.ja.md#3-セキュリティ編--つながった相手をどこまで信頼するか) — pairing・bond・IO capability

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[RuntimePasskeyClient.ino](RuntimePasskeyClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[RuntimePasskeyServer](../RuntimePasskeyServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

RuntimePasskeyClient - Passkey Entryの入力側。接続するとPairingが始まり、 スタックがpasskeyを要求する。sketchはSerialから入力された値を providePasskey() で答える。相手の表示を読んで人間が打ち込む場面の再現。

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

以前のペアリング時の値ではなく、今回Serverが表示したパスキーを入力します。入力待ちでもupdateを回し、セキュリティ完了イベントを確認します。

Pairingはセキュリティを確立し、Bondingは次回のために信頼情報を保存します。リセットしても毎回確認が出るとは限りません。encrypted・authenticated・bondedを分けて確認し、利用者の回答待ちでもupdateを回します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
