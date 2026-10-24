# NusClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

汎用GATT Client APIの組合せでNordic UART Service（NUS）Serverと通信するCentralです。Service `6e400001-…` のもとで、TX Notification（`6e400003-…`）を購読し、Serialへ入力された各行をWrite Without ResponseでRX（`6e400002-…`）へ送ります。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [NusServer](../NusServer/) exampleを動かすESP32-S3 × 1（またはNUS互換のPeripheral）

## 動作

- NUS Service UUIDをscanして接続します
- 接続後にTX Notificationを購読し、`NUS ready`を表示します
- Serialから空でない各行を読み取り、RXへWrite Without Responseで送ります
- Serverから返るTX Notificationを`RX: …`として表示します

## 主なAPI

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid)` — TX Notificationを購読
- `ble.onSubscribed(callback)` — 購読完了（`result.success`）
- `ble.writeCharacteristic(connectionId, serviceUuid, characteristicUuid, value, false)` — RXへWrite Without Response
- `ble.onCharacteristicWritten(callback)` — Write受理の結果
- `ble.onNotification(callback)` — `characteristicUuid.equalsIgnoreCase(...)`で絞り込むTX payload

## メモ

- NUSが汎用GATT Client APIの組合せで構築できることを示します。stream意味論や自動packet framingは提供しません。Serialモニタに行を入力すると送信されます。

## 期待されるSerial出力

```
NUS ready: 1
TX accepted: 1
RX: hello
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[NusClient.ino](NusClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[NusServer](../NusServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.subscribe(...)` — 相手のCharacteristic更新を購読します。NotifyとIndicateの購読方式を合わせ、購読の完了を待ちます。
4. `ble.onSubscribed(...)` — result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
5. `ble.onCharacteristicWritten(...)` — 指定したWrite方式での結果をresult.successで確認します。コマンドの実行確認が必要なら、アプリ側の応答も用意します。
6. `ble.onNotification(...)` — Characteristicと接続を絞り、バイナリ形式を検証してから更新をデコードします。IndicateのデータもこのAPIへ届きます。
7. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
8. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
9. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。
10. `ble.writeCharacteristic(...)` — 相手へのWriteをキューへ登録し、完了イベントを確認します。Write Without Responseには、相手アプリの処理を保証するATT応答はありません。

## 確認と変更のポイント

NUSは2つのCharacteristicで、自動的なシリアルストリームではありません。例はシリアルの行を使います。バイナリや長いメッセージには区切りと分割方法を設計し、購読成功を準備完了として扱います。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
