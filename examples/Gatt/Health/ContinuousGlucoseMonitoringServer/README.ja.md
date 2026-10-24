# ContinuousGlucoseMonitoringServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Continuous Glucose Monitoring Service（0x181F）のPeripheral。CGM Feature（0x2AA8）はEnd-to-End CRCで保護されたread値、CGM Measurement（0x2AA7）はSFLOAT血糖値、time offset、末尾のE2E-CRC付きで**Notify**します。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [ContinuousGlucoseMonitoringClient](../ContinuousGlucoseMonitoringClient/) example、または CGM collector

## 動作

- `begin()`の前にCGM MeasurementとCGM Featureを登録し、0x181FをAdvertise
- E2E-CRC対応ビットをセットしたCGM Feature（0x001000）＋Type/Sample Locationを、末尾のE2E-CRCで保護して公開
- 3秒ごとに、CGM Measurement（Size、Flags、100 mg/dL付近のSFLOAT血糖値、time offset）を末尾のE2E-CRC付きでNotify

## 主なAPI

- `espBleCgmAppendCrc(buffer, length)` — `EspBleCgmCrc.h`のCGM E2E-CRC（CRC-16/MCRF4XX）を付加
- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 SFLOAT血糖値をエンコード

## 期待されるSerial出力

Server側は出力しません。血糖値はClientで確認します。

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[ContinuousGlucoseMonitoringServer.ino](ContinuousGlucoseMonitoringServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[ContinuousGlucoseMonitoringClient](../ContinuousGlucoseMonitoringClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

ContinuousGlucoseMonitoringServer - 標準CGM Service（0x181F）。CGM Feature （0x2AA8）はE2E-CRCで保護されたread値、CGM Measurement（0x2AA7）はSFLOAT 血糖値、time offset、末尾のE2E-CRC（CRC-16/MCRF4XX、EspBleCgmCrc.h）付きで Notifyする。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `server.addService(...)` — BLEのbegin前にローカルServiceを作ります。Service UUIDはCharacteristicのまとまりを識別するもので、個体の識別子ではありません。
2. `server.addCharacteristic(...)` — BLEのbegin前に値と許可する操作を登録します。Read・Write・Notify・Indicateはそれぞれ別の能力です。
3. `server.setValue(...)` — ローカルに保持するCharacteristic値を変更します。これだけで購読中の相手へNotifyやIndicateが送られるわけではありません。
4. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
5. `ble.advertising().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。
6. `ble.gattServer().notify(...)` — 購読者への更新をキューへ登録し、受理を確認します。Notifyには受信側からのATT確認応答がないため、必要に応じ送信結果やアプリの応答を使います。

## 確認と変更のポイント

E2E-CRCもパケット形式の一部です。サンプルのバイトを変えたら再計算し、受信時はSFLOAT濃度や時刻差をデコードする前に検証します。

Serverの値はプロトコルを示す模擬値で、医療センサーの実測値ではありません。実データの取得は別に置き換え、サービスの符号化とClient側の検証を保ちます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../../README.ja.md) · [サンプル実行ガイド](../../../../docs/GUIDE_EXAMPLES.ja.md)
