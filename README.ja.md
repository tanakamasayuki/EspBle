# EspBle

> English: [README.md](README.md)

ESP32 Arduino向けの汎用Bluetooth Low Energyライブラリです。**Arduino-ESP32に同梱されたNimBLE backendを使用し、Bluetooth Classicには対応しません。** Central / Peripheral、GATT Client / Server、Security、標準プロファイルと独自GATTサービスを共通基盤上で扱います。外部のNimBLE-Arduinoは必須依存にしません。

> [!IMPORTANT]
> **無印ESP32（classic）では動作しません。** EspBleはNimBLE backendを必要としますが、無印`esp32`ビルドはNimBLEを同梱せず（Bluedroidが既定）、設計上コンパイルできません。対応ターゲットはNimBLEを同梱するSoC（**ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-H2**）です。無印ESP32でBLEを使いたい場合は、代わりに [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) を利用してください（[対応環境](#対応環境)を参照）。

公開APIはまだ確定していません。初回リリース前の試行段階で、APIは変更される可能性があります。

## 機能

- Legacy AdvertisingとScanning、値型のScan Result
- 安定したlibrary connection IDを持つCentral / Peripheral接続
- 汎用GATT Server / Client: 既知UUID Discovery、Read、Write、Notify / Indicate、購読
- MTU交換、Connection snapshot、payload上限検証
- Security: Just Worksと静的passkey Pairing（LE Secure Connections）、Bonding、暗号化/認証Characteristic permission
- HID Keyboard Device: HID / Device Information / Battery Serviceを合成した固定6KRO Report Protocol keyboard
- HID Keyboard Host: Report Map解析、Input購読、256-bit usage snapshot、EspUsbHost互換19 layout（Unicode変換）のキーイベント、LED出力
- ユーザーcallbackはすべてloop task上の`ble.update()`から配送されます（BLE stack taskからは呼ばれません）

上記はすべてESP32-S3 2台の自動Peerテストとhost上のunit testで検証しています。詳細は[テスト計画](tests/TEST_PLAN.ja.md)を参照してください。

## 対応環境

EspBleは**Arduino-ESP32同梱のNimBLE backend**を必要とします。Bluedroidが既定のcore（無印`esp32`ボードなど）はコンパイル時に`#error`で明示的に拒否します。したがって無印ESP32は本ライブラリの**対象外**です。無印ESP32でBLEを使いたい場合は、NimBLEホストスタックを自前で同梱していて無印ESP32でも動作する [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) を利用してください（EspBleとはAPIが異なります）。

開発とPeerテストはarduino-esp32 3.3.10で行っています。対応するcoreバージョンの範囲とボードごとのビルドカバレッジは手動管理ではなくCIで計測します:

- **Core Compatibility Matrix** ワークフロー → `docs/COMPATIBILITY.<version>.md`（ESP32-S3で代表exampleをarduino-esp32の各リリースに対してビルド）
- **Board Build Coverage** ワークフロー → `docs/BOARDS.<version>.md`（1つのcoreバージョンで全exampleをESP32-S3 / ESP32 / C3 / C6 / H2 / P4に対してビルド）

どちらもフルsweepが全sketchを書き換えて再ビルドするため、手動実行（`workflow_dispatch`）です。確定した最小coreバージョンは生成されたマトリクスを参照してください。

## はじめかた

各exampleには検証済みArduino-ESP32バージョンを固定した`sketch.yaml`が同梱されています:

```sh
arduino-cli compile --profile esp32s3 examples/Gap/Scan
```

全examplesの一覧と組み合わせは[examplesの目次](examples/README.ja.md)を参照してください。最小のスキャナは次のとおりです:

```cpp
#include <EspBle.h>

EspBle ble;

void setup() {
  Serial.begin(115200);
  ble.begin();
  ble.scanner().onResult([](const EspBleScanResult &result) {
    Serial.printf("%s RSSI=%d\n", result.address.c_str(), result.rssi);
  });
  ble.scanner().start();
}

void loop() {
  ble.update();  // すべてのcallbackはここから配送されます
  delay(1);
}
```

## 文書

- [開発状況とTODO](docs/STATUS.ja.md)
- [要件](docs/REQUIREMENTS.ja.md)
- [コア設計](docs/CORE_DESIGN.ja.md)
- [API設計](docs/API_DESIGN.ja.md)
- [HID Keyboard Device仕様](docs/HID_KEYBOARD_DEVICE_SPEC.ja.md)
- [HID Keyboard Host仕様](docs/HID_KEYBOARD_HOST_SPEC.ja.md)
- [用語と命名規則](docs/TERMINOLOGY.ja.md)
- [設計決定](docs/DECISIONS.ja.md)
- [機能対応マトリクス](docs/FEATURE_MATRIX.ja.md)
- [開発計画](docs/DEVELOPMENT_PLAN.ja.md)
- [テスト計画](tests/TEST_PLAN.ja.md)

## 関連ライブラリ

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB Hostライブラリ。EspBleはkeyboard layout tableとHID usageの扱いを共有しています
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — 組み合わせテストに使用するUSB Deviceライブラリ

## ライセンス

MIT License
