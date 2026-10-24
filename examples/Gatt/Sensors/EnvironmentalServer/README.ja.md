# EnvironmentalServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Environmental Sensing Service（0x181A）のPeripheralです。Temperature（0x2A6E）、Humidity（0x2A6F）、Pressure（0x2A6D）を仕様のlittle-endian整数スケール（0.01 ℃、0.01 %、0.1 Pa）で公開します。TemperatureはRead / Notify可能、humidityとpressureはRead専用です。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [EnvironmentalClient](../EnvironmentalClient/) example、または任意のBLE Central/スマートフォン

## 動作

- Environmental Sensing serviceを登録し、0x181Aをadvertise
- Temperature 21.50 ℃、Humidity 48.75 %、Pressure 101325.0 Pa をReadable値として設定
- Serialを読み取り、`+` / `-` を送ると温度を0.25 ℃変更して再設定し、新しい値をNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .readable = true, .notifiable = true })` — Temperature
- `ble.gattServer().setValue(...)` — 保持しているcharacteristic値を更新
- `ble.gattServer().notify(...)` — 購読者へNotify。戻り値で受理されたかを確認できる

## メモ

- `notify()`は、まだ誰もTemperatureを購読していないとき0を、購読者が接続済みのとき1を返します。

## 期待されるSerial出力

```
Send '+' or '-' to change temperature by 0.25 C.
Temperature raw: 2175 (notification accepted: 1)
Temperature raw: 2200 (notification accepted: 1)
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[EnvironmentalServer.ino](EnvironmentalServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[EnvironmentalClient](../EnvironmentalClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
2. `server.notify(...)` — 購読者への更新をキューへ登録し、受理を確認します。Notifyには受信側からのATT確認応答がないため、必要に応じ送信結果やアプリの応答を使います。
3. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
4. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
5. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
6. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

温度・湿度・気圧では整数の幅と倍率が異なります。例の温度はシリアル操作で変わります。実センサーへ置き換える際は、センサー出力を既存の通信形式へ変換します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
