# BondManagementServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Bond Management Service（0x181E）のPeripheral。Bond Management Feature（0x2AA5）は対応操作のread可能なuint24 bit field、Bond Management Control Point（0x2AA4）はwritableでop codeを`onWritten`で受け取ります。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [BondManagementClient](../BondManagementClient/) example、または Bond Management client

## 動作

- `begin()`の前にControl PointとFeatureを登録し、0x181EをAdvertise
- 「Delete bond of requesting device（LE）」（bit 0）と「全bond削除（LE + BR/EDR）」（bit 4）対応を提示 → 0x000011
- op code 0x03（Delete bond of requesting device、LE）で、そのpeerのbondを切断後に削除（アクティブなリンクを壊さないよう遅延）

## 主なAPI

- `ble.gattServer().onWritten(...)` — Control Pointのop codeを受信
- `ble.bondCount()` / `ble.bond(i, bond)` / `ble.deleteBond(bond)` — bondの列挙と削除

## 期待されるSerial出力

```
Bond Management op code: 3
Bond deleted
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[BondManagementServer.ino](BondManagementServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[BondManagementClient](../BondManagementClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

BondManagementServer - 標準Bond Management Service（0x181E）。Bond Management Feature（0x2AA5）は対応操作のread可能なuint24 bit field、Bond Management Control Point（0x2AA4）はwritableでop codeをonWrittenで受け取る。 「Delete bond of requesting device」（0x03）では、切断後にそのpeerのbondを 削除する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `server.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `server.onWritten(...)` — updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
5. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
6. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
7. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
8. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

Control Pointは表示値だけでなく保存されたbondを変更します。対応Featureビットとコードで実装したopcodeを確認し、この例を公開管理サービスの認可方針としてそのまま使わないようにします。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
