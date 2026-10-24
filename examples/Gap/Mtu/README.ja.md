# Mtu

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

接続前に大きめのATT MTUを希望値として設定し、交換結果を観察します。希望MTUは`begin()`へ渡すconfigで指定し、同梱NimBLE backendが接続確立時に交換します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central）
- BLE Peripheral × 1 — このsketchは[Gatt/Basics/NotifyServer](../../Gatt/Basics/NotifyServer/) exampleのService UUIDをscanするので、2台目のボードでNotifyServerを動かしてください

## 動作

- `begin()`前に`config.preferredMtu = 185`を設定します
- NotifyServerのService UUIDをadvertiseする最初の相手へ接続します
- 交換されたMTUと、そこから決まるNotification payload上限（`mtu - 3`）を表示します
- MTU変更イベントを変更前後の値と一緒に表示します

## 主なAPI

- `EspBleConfig::preferredMtu` — 希望ATT MTU（23〜517）。範囲外は`begin()`が`InvalidArgument`で拒否します
- `connection.mtu` — そのイベント時点のMTU。**接続直後は23**です。MTUの交換は接続が成立した直後に行われるので、`onConnected`の時点ではまだ既定値で、交換結果は`onMtuChanged`で届きます（CentralとPeripheralのどちらも同じ順序です）
- `connection.maximumNotificationPayload()` — `mtu - 3`（ATT notification header分を除いた値）
- `ble.onMtuChanged(callback)` — `event.previousMtu`と`event.connection.mtu`

## メモ

- Central側のMTUは接続時のsnapshotです。接続後にMTUが変わっても追跡できません。同梱backendがGATT client側にMTU変更の通知経路を持たず、EspBleが変化を知る手段がないためです。

## 期待されるSerial出力

```
Connected with MTU 185 (notification payload up to 182 bytes)
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Mtu.ino](Mtu.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

Mtu - 接続前に大きめのATT MTUを希望値として設定し、交換結果を観察する。 希望MTUは begin() へ渡すconfigで指定し、backendが接続確立時に交換する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
4. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

希望値がそのまま採用されるとは考えず、MTUイベントで交渉結果を確認します。ATTの値に使える容量はMTUより小さく、大きなMTU自体がメッセージの区切りを定義するわけではありません。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
