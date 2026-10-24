# MultiConnection

> English: [README.md](README.md)
> 概念: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md)

1つのCentralが複数のPeripheral接続を同時に保持する例です。接続ごとにIDがあり、
すべての操作は対象の接続を明示します。このlibraryに「現在の接続」という暗黙の対象は
ありません。

## 必要なもの

- このsketchを動かすESP32-S3 1台（Central）
- Battery Service `0x180f`をadvertiseするperipheral 2〜3台。例えば
  [Gatt/Device/BatteryServer](../../Gatt/Device/BatteryServer/)を動かすboard

## 動作

- 接続ごとにscanを再開し、最初の1台で止めない。満杯になったら収集をやめる——
  さもないと満杯のCentralがhostに拒否される接続を試み続ける
- 各peerからbattery levelを読む。共通の結果callbackが、どの接続の応答かを持っている
- 切断時はIDの一致でエントリを外す。IDは1つの接続に一生対応する

## 主なAPI

- `ble.connectionCount()` — 現在保持している数
- `ble.readCharacteristic(connectionId, service, characteristic)` — 接続が
  呼び出しの一部になっている
- `ble.onCharacteristicRead()` — 全接続で共通のcallback。結果が接続IDを持つ

## 制限

無印ESP32の同梱hostでは同時接続3までです。他のtargetはもっと多く扱えます。

## Serialコマンド

| キー | 動作 |
|---|---|
| `r` | 各peerのbattery levelを読む |
| `d` | 最初のpeerを切断 |

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[MultiConnection.ino](MultiConnection.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

1つのCentralが複数のPeripheral接続を同時に保持する例。接続ごとにIDがあり、 すべての操作は対象の接続を明示する。このlibraryに「現在の接続」という暗黙の 対象は無い。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
4. `ble.onCharacteristicRead(...)` — 成功・UUID・値の長さを確認してからデコードします。バイナリのStringには0バイトも含まれるので、長さを使って処理します。
5. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
6. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。
7. `ble.readCharacteristic(...)` — 相手の値のReadをキューへ登録します。値は即時のboolではなくonCharacteristicReadへ届きます。

## 確認と変更のポイント

接続IDごとに状態を持ち、Read・通知・切断のIDを確認します。複数の相手には、1つだけのreadyフラグやhandleでは不十分です。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
