# ConnectionInspector

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

対話式の診断ツールです。周囲のconnectableな機器を番号つきで一覧表示し、選んだ相手へ接続してConnection snapshotをすべてダンプします: connection ID、backend handle、peer addressと種別、local role、交換済みMTU（とNotification payload上限）、security状態（encrypted / authenticated / bonded / 鍵長）。Bond storeと診断カウンタのダンプもできます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central）
- 調べたい周囲のBLE Peripheral（advertise中の任意の機器）

## 動作

- scanして最大10台のconnectableな機器を`[index] address rssi name`形式で一覧します
- `0`〜`9`でその番号の機器へ接続し、Connection snapshotを表示します
- `s`で一覧をクリアして再scan、`d`で現在の接続を切断、`b`でBond storeをダンプ、`q`で診断カウンタを表示します
- security無効なので、暗号化必須のPeripheralでも接続自体は成立してlink情報を確認できます（attributeへのアクセスは拒否されます）

## 主なAPI

- `EspBleConnection` — `id`、`handle`、`peerAddress`、`peerAddressType`、`localRole`、`mtu`、`maximumNotificationPayload()`、`encrypted`、`authenticated`、`bonded`、`encryptionKeySize`
- `ble.connect(scanResult)` / `ble.disconnect(connectionId)` / `ble.onConnectionFailed(callback)`
- `ble.bondCount()` / `ble.bond(index, out)` — Bond storeのsnapshotアクセス
- `ble.connectionCount()`、`ble.droppedEventCount()`、`ble.scanner().droppedResultCount()`

## 期待されるSerial出力

```
Commands: 0-9 connect to listed device, s rescan, d disconnect, b bonds, q counters
SCAN restart success=1 - send the list number to connect
[0] 5a:b8:1e:0c:2f:71 rssi=-52 name=EspBle Keyboard
CONNECT [0] 5a:b8:1e:0c:2f:71 accepted=1
CONNECTION id=1 handle=0 peer=5a:b8:1e:0c:2f:71(type=0) role=Central
  mtu=255 maxNotificationPayload=252
  encrypted=0 authenticated=0 bonded=0 keySize=0
```

## 関連するガイド

- [BLE入門ガイド §1 BLEとは](../../../docs/GUIDE_BLE_BASICS.ja.md#1-bleとは) — 以降の文書が使う語彙

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[ConnectionInspector.ino](ConnectionInspector.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

ConnectionInspector - 対話式の診断ツール。周囲のconnectableな機器を一覧し、選んだ 相手へ接続してConnection snapshot（MTU・role・security状態）をダンプする。Bond store と診断カウンタも表示する。security無効なので、暗号化必須のPeripheralでも接続自体は成立し link情報を確認できる（Readは拒否される）。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
4. `ble.onConnectionFailed(...)` — 接続要求が受理されたあとに発生する、相手不在や接続拒否などの失敗を確認します。
5. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
6. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

接続一覧と保存bond一覧を別々に確認します。通信量を増やしてイベント破棄カウンタを見ます。接続一覧から消えてもbondは削除されません。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
