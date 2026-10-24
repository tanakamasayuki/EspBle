# ProximityServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Proximityプロファイルの**Reporter**役。2つの標準Serviceを同時にホストします: Link Loss Service（0x1803、read/writeのAlert Level 0x2A06）と、Tx Power Service（0x1804、read専用のsigned int8 Tx Power Level 0x2A07）。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral / Proximity Reporter）
- 1 × Central: [ProximityClient](../ProximityClient/) example、または Proximity Monitor

## 動作

- `begin()`の前に両Serviceを登録し、0x1803をAdvertise
- 固定のTx Power Level -8 dBmを提示
- 書かれたLink Loss Alert Levelを`onWritten`で保存し、read backできるようにする（実機のReporterはこのlevelに従いlink loss時に鳴動する）

## 主なAPI

- 1つのserverに対する2回の`addService(...)`
- `ble.gattServer().onWritten(...)` — Alert Levelの書き込みを受信

## 期待されるSerial出力

```
Link Loss Alert Level: 2 (High Alert)
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[ProximityServer.ino](ProximityServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[ProximityClient](../ProximityClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

ProximityServer - ProximityプロファイルのReporter役。2つの標準Serviceを 同時にホストする: Link Loss Service（0x1803、read/writeのAlert Level 0x2A06）と、Tx Power Service（0x1804、read専用のsigned int8 Tx Power Level 0x2A07）。実機のReporterは設定されたAlert Levelに従いlink loss時に鳴動する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `server.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `server.onWritten(...)` — updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
5. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
6. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

Tx Powerは申告された送信電力、Link Loss Alert Levelは設定値です。校正済みの距離測定ではありません。切断時に実物を動かす処理が必要なら別に実装します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
