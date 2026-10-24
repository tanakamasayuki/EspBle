# AutoReconnectClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

[Gatt/NotifyServer](../NotifyServer/) exampleへ接続し、一度だけ購読します。auto-reconnectを有効にすると、persistent subscription（既定on）と併せて、想定外の切断後にリンクと購読が自動復元され、追加コードなしでNotificationが再開します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- [Gatt/NotifyServer](../NotifyServer/) exampleを動かすESP32-S3 × 1

## 動作

- NotifyServerのService UUIDをscanして接続します
- 一度だけ購読し、`setAutoReconnect(true)`を呼びます
- 想定外の切断時、ライブラリが同じpeer addressへ自動再接続し、購読も自動で復元（persistent subscription）するため、Notificationが再開します

## 主なAPI

- `ble.setAutoReconnect(true)` — connect済みの全Central peerを記憶し、想定外の切断時に自動再接続する。`disconnect()`は意図的な切断として再接続対象から除外する
- `EspBleConfig::persistentSubscriptions` — 既定on。`subscribe()`成功をpeerごとに記憶し、再接続時に復元する
- `ble.onConnected` / `ble.onDisconnected` / `ble.onSubscribed` / `ble.onNotification`

## 期待されるSerial出力

```
Connected to ...
Subscription active (restored automatically after a reconnect).
Notification: 1
Notification: 2
Disconnected - auto-reconnect will restore the link.
Connected to ...
Subscription active (restored automatically after a reconnect).
Notification: 3
...
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[AutoReconnectClient.ino](AutoReconnectClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[NotifyServer](../NotifyServer/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

AutoReconnectClient - Gatt/NotifyServer example へ接続し一度だけ購読する。 auto-reconnectを有効にすると、persistent subscription（既定on）と併せて、 切断時にライブラリが同じpeerへ自動再接続し購読も自動復元するため、追加の コードなしでNotificationが再開する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.setAutoReconnect(...)` — 再接続方針を明示します。手動スキャンと自動リトライが競合しないよう、再接続を担当する処理を決めます。
3. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
4. `ble.subscribe(...)` — 相手のCharacteristic更新を購読します。NotifyとIndicateの購読方式を合わせ、購読の完了を待ちます。
5. `ble.onSubscribed(...)` — result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
6. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
7. `ble.onNotification(...)` — Characteristicと接続を絞り、バイナリ形式を検証してから更新をデコードします。IndicateのデータもこのAPIへ届きます。
8. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
9. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

対応Serverと切断・再接続の一巡を確認します。購読の復元と再接続方針は別の仕組みです。古い接続IDを使わず、各接続で利用可能状態を確認します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
