# Hosted / CustomPins

> English: [README.md](README.md)

ESP32-P4とESP-Hosted co-processor間のSDIO pinを、Arduino Coreの
`hostedSetPins()`で実行時に上書きするexampleです。コード中の値はM5Stack Tab5の
配線です。

通常はArduino IDEで実機に合うボードを選ぶのが推奨です。たとえばTab5を
`M5Tab5`（CLIでは`esp32:esp32:m5stack_tab5`）としてbuildすると、Coreの
`variants/m5stack_tab5/pins_arduino.h`にあるpinが自動使用され、上書きは不要です。

このexampleは次の場合に使用します。

- 独自基板でP4とC6を配線した
- 使用中のArduino Coreにboard variantがまだ無い
- generic ESP32-P4 board定義のpin配置と実機が異なる

## 重要な順序

`hostedSetPins()`は、共有ESP-Hosted transportが初期化される前に呼ぶ必要があります。

```cpp
hostedSetPins(clk, cmd, d0, d1, d2, d3, reset);
ble.begin();
```

Wi-Fiを先に開始する場合は、`WiFi.STA.begin()`より前に設定します。初期化後の変更、
負数、7本の一部だけの指定はArduino Coreに拒否されます。設定はRAM上だけに保持され、
再起動するとboard variantの値へ戻ります。

```sh
# generic P4定義を使い、sketch側でTab5配線へ上書きする
arduino-cli compile --profile esp32p4 examples/Hosted/CustomPins

# Tab5 variant自体のbuildも確認する
arduino-cli compile --profile m5stack_tab5 examples/Hosted/CustomPins
```

pin設定はWi-FiとBLEが共有するSDIO transportの設定であるため、EspBle独自APIではなく
Arduino CoreのHosted HALを使用します。詳細は
[ESP-Hostedセットアップ](../../../docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き)を
参照してください。

## 関連するガイド

- [ESP-Hostedセットアップ](../../../docs/ESP_HOSTED_SETUP.ja.md) — 配線・対応version・C6 firmware
- [ESP-Hostedの実機確認済み制限](../../../docs/ESP_HOSTED_LIMITATIONS.ja.md) — いま動かないものとその理由

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[CustomPins.ino](CustomPins.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

ESP32-P4のESP-Hosted SDIO端子を指定する例です。通常は適切なボード定義を選び、独自基板などで必要な場合だけ上書きします。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。

## 確認と変更のポイント

BLEまたはWi-Fiが共有Hosted Transportを初期化する前にSDIO端子を設定します。基板固有の端子群はまとめて置き換えます。無線相手のない基板へソフトだけでBluetooth機能を追加する例ではありません。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
