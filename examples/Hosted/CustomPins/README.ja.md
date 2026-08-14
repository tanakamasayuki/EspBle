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
