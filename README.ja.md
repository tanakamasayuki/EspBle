# EspBle

> English: [README.md](README.md)

ESP32 Arduino向けの汎用Bluetooth Low Energyライブラリです。**Arduino-ESP32に同梱されたNimBLEホストAPIを直接使用し（同梱の`BLE`ラッパクラスには依存しません）、Bluetooth Classicには対応しません。** Central / Peripheral、GATT Client / Server、Security、標準プロファイルと独自GATTサービスを共通基盤上で扱います。外部のNimBLE-Arduinoは必須依存にしません。

> [!IMPORTANT]
> **無印ESP32（classic）では動作しません。** EspBleはNimBLE backendを必要とします。内蔵BLE Controller構成の対象は**ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-H2**です。**ESP32-P4 + ESP32-C6のESP-Hosted構成は基本GAP/GATT機能のみ制限付きで対応**し、Security/bondingと複数回の完全再初期化には上流の既知制限があります。無印ESP32でBLEを使いたい場合は[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)を利用してください（[対応環境](#対応環境)を参照）。

1.0.0リリース前のため公開APIはまだ確定しておらず、破壊的に変更する可能性があります。

## 機能

- Legacy AdvertisingとScanning、値型のScan Result、non-connectable Beacon、iBeacon（`EspBleIBeacon.h`）、Service Data送受信
- Scan Resultまたはaddressを指定するCentral接続と、安定したlibrary connection IDを持つPeripheral接続。複数同時接続と、想定外の切断時に自動復旧するauto-reconnect（`setAutoReconnect`、既定off）
- 汎用GATT Server / Client: 接続ごとのDatabase一覧・既知UUID Discovery、Characteristic/Descriptor Read・Write、操作timeoutと自動キュー、Notify / Indicate、購読（再接続時に自動再購読するpersistent subscription、既定on）
- MTU交換、Connection snapshot、payload上限検証
- Security: Just Worksと静的passkey Pairing（LE Secure Connections）、Bonding、暗号化/認証Characteristic permission
- Address privacy: public / random static / Resolvable Private Address（RPA）を`EspBleConfig::ownAddressType`で選択
- 複合HID Device: 6KRO/NKRO keyboard、mouse、consumer/system control、gamepad、Vendor Input / Output / Featureを1つのHID / Device Information / Battery Service群へ合成
- HID Host: 全対応Reportを横断Discoveryして種別別eventへ配送。keyboardは6KRO/NKRO解析、256-bit usage snapshot、19 layout、LED出力、Vendorは双方向Reportに対応
- BLE MIDI Device / Host: timestamp・running status・SysEx対応のpacket codecと、EspUsbDevice/EspUsbHostのMIDI APIに揃えた`EspBleMidiDevice` / `EspBleMidiHost` helper
- ユーザーcallbackはすべてloop task上の`ble.update()`から配送されます（BLE stack taskからは呼ばれません）

上記の全機能はESP32-S3 2台の自動Peerテストとhost上のunit testで検証しています。P4/C6 Hosted構成は接続、GATT、notify/indicate、MTUなどの基本機能をP4/S3で検証中です。詳細は[テスト計画](tests/TEST_PLAN.ja.md)を参照してください。

## 対応環境

EspBleは**Arduino-ESP32同梱のNimBLE backend**を必要とします。Bluedroidが既定のcore（無印`esp32`ボードなど）はコンパイル時に`#error`で明示的に拒否します。したがって無印ESP32は本ライブラリの**対象外**です。無印ESP32でBLEを使いたい場合は、NimBLEホストスタックを自前で同梱していて無印ESP32でも動作する [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) を利用してください（EspBleとはAPIが異なります）。

ESP32-P4はArduino-ESP32が提供するESP-Hosted NimBLE構成で利用できる。検証済みの
Host/Slave version、C6更新方法、対応済み範囲は
[ESP-Hostedセットアップ](docs/ESP_HOSTED_SETUP.ja.md)を参照する。Core 3.3.11では
同梱IDFのP4 ECC不具合によりLE Secure Connections、bonding、それを前提とするHID、
および`end()`後の複数回の再`begin()`には
[既知制限](docs/ESP_HOSTED_LIMITATIONS.ja.md)がある。

開発とPeerテストはarduino-esp32 3.3.11で行っています。対応するcoreバージョンの範囲とボードごとのビルドカバレッジは手動管理ではなくCIで計測します:

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

**BLEがはじめての方は[BLE通信の入門ガイド](docs/GUIDE_BLE_BASICS.ja.md)から** — 相手を探すところからデータのやり取りまで、何が起きているのかを説明し、話題ごとに対応するexampleへ案内します。

**特定の文書を探すときは[ドキュメント案内](docs/README.ja.md)へ** — 読む順序と各文書の役割をまとめています。「今どこまで進んでいるか」を最短で把握するには [docs/STATUS.ja.md](docs/STATUS.ja.md) → [docs/DECISIONS.ja.md](docs/DECISIONS.ja.md) の順です。

- [BLE通信の入門ガイド](docs/GUIDE_BLE_BASICS.ja.md)
- [開発状況とTODO](docs/STATUS.ja.md)
- [要件](docs/REQUIREMENTS.ja.md)
- [コア設計](docs/CORE_DESIGN.ja.md)
- [API設計](docs/API_DESIGN.ja.md)
- [HID Device仕様](docs/HID_DEVICE_SPEC.ja.md)
- [HID Host仕様](docs/HID_HOST_SPEC.ja.md)
- [用語と命名規則](docs/TERMINOLOGY.ja.md)
- [設計決定](docs/DECISIONS.ja.md)
- [機能対応マトリクス](docs/FEATURE_MATRIX.ja.md)
- [テスト計画](tests/TEST_PLAN.ja.md)
- [リリースチェックリスト](docs/RELEASE_CHECKLIST.ja.md)
- [ESP32-P4 / ESP-Hostedセットアップ](docs/ESP_HOSTED_SETUP.ja.md)
- [ESP32-P4 / ESP-Hostedの既知制限](docs/ESP_HOSTED_LIMITATIONS.ja.md)

## 関連ライブラリ

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB Hostライブラリ。EspBleはkeyboard layout tableとHID usageの扱いを共有しています
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — 組み合わせテストに使用するUSB Deviceライブラリ

## ライセンス

MIT License
