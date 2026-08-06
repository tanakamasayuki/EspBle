# EspBle

> English: [README.md](README.md)

ESP32 Arduino向けの汎用Bluetooth Low Energyライブラリです。**Arduino-ESP32
CoreへESP-IDF componentとして組み込まれているNimBLE Host APIを直接呼び出します。**
Arduino-ESP32同梱の`BLEDevice` / `BLEClient` / `BLEServer`などのラッパを経由せず、
Central / Peripheral、GATT Client / Server、Security、HID、BLE MIDIを1つの
`EspBle`基盤上で扱います。Bluetooth Classicには対応しません。

> [!IMPORTANT]
> EspBleはArduino-ESP32 Core内のNimBLE backendを使用します。内蔵BLE Controller構成の
> 対象は**ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-H2**です。
> **ESP32-P4 + ESP32-C6はESP-Hosted経由で制限付き対応**し、Security/bondingと
> 複数回の完全再初期化には上流の既知制限があります。
> **無印ESP32（classic）は、EspBleがNimBLE Hostを同梱することで動きますが、
> 推奨は兄弟ライブラリ[EspBleBluedroid](https://github.com/tanakamasayuki/EspBleBluedroid)です**
>（[対応環境](#対応環境)を参照）。

## なぜEspBleを使うのか

- **Coreに統合されたNimBLEをそのまま使う:** Arduino-ESP32が選定・buildした
  ESP-IDFのNimBLE Host、ControllerまたはESP-Hosted HCI構成を直接利用します。
  別のBLE stackをライブラリ内へ重ねず、CoreのSoC対応・設定・更新に追従します。
- **低レベルの正確さをArduino向けAPIで扱う:** GATT attributeをUUIDだけでなく
  handleでも指定でき、同一UUIDのService/Characteristic、Descriptor、接続ごとの
  discovery snapshotを区別できます。非同期GATT操作はtimeout付きqueueで直列化されます。
- **CentralとPeripheralを同じ設計で組み合わせる:** scan、advertising、複数接続、
  GATT Client/Server、pairing/bondingを同じ`EspBle` instanceで構成できます。
- **HIDとBLE MIDIを一から組まなくてよい:** Keyboard、Mouse、Consumer/System Control、
  Gamepad、Custom HIDを1つのHID Serviceへ合成でき、HID HostとBLE MIDIの
  Device/Host helperも同じeventモデルで利用できます。
- **callbackの実行場所が予測可能:** 接続、GATT操作完了、通知、HIDなどの非同期eventは、
  `ble.update()`を呼ぶloop taskから配送します。同期応答が必要なGATT Serverの
  `onRead()`だけはstack task上で動く明示的な例外です。
- **実機testで振る舞いを固定:** 2台のESP32を使うPeer testで接続、GATT、Security、
  HID、再接続、異常系を検証し、host unit testと複数SoCのexample buildも実行します。

## 機能

| 分野 | できること | 主な挙動・API |
| --- | --- | --- |
| Advertising / Scan | 通常のAdvertising、active/passive scan、non-connectable Beacon、iBeacon、Service Data | Scan Resultはaddress、name、RSSI、Service UUID、Manufacturer Dataなどを保持する値型。Advertising payloadとScan Responseを個別に構成可能 |
| 接続管理 | CentralからScan Result/addressへ接続、Peripheralとして接続受け入れ、切断、複数同時接続 | アプリ向けの安定したconnection ID、接続状態snapshot、接続parameter/PHY更新、想定外切断からのauto-reconnect（既定off） |
| GATT Server | 独自Service、Characteristic、Descriptorの登録、Read/Write、Notify/Indicate | UUIDまたはattribute handleで対象を識別。同一UUIDのService/Characteristicにも対応。接続別の購読状態を追跡 |
| GATT Client | Database全体または既知UUIDのDiscovery、Characteristic/Descriptor Read/Write、購読/解除 | 接続ごとのdiscovery snapshot、handle指定、long read、timeout、自動operation queue。persistent subscriptionは再接続時に自動復元（既定on） |
| ATT / Link | MTU交換、payload上限検証、Connection Parameter、LE PHY | MTUと接続状態をconnection snapshotへ反映し、非同期の完了eventとして通知 |
| Security / Privacy | LE Secure ConnectionsによるJust Works・passkey・Numeric Comparison、Bonding、暗号化/認証permission | public / random static / Resolvable Private Address（RPA）を選択可能。P4/C6 HostedのSecurityは既知制限あり |
| HID Device | Keyboard（6KRO/NKRO）、Mouse、Consumer/System Control、Gamepad、Vendor、任意Custom Report | 複数profileを1つのHID Serviceへ合成。Report送信、Battery、LED Output、Boot Keyboard Protocolを提供 |
| HID Host | BLE keyboard/mouse/gamepadなどのDiscovery、購読、Report解析 | 6KRO/NKROをusage snapshotへ正規化、19 keyboard layout、LED出力、Vendor Input/Output/Feature Reportに対応 |
| BLE MIDI | MIDI Device / Host、Note、Control Change、Program Change、Pitch Bend、SysEx | timestamp、running status、複数BLE packetにまたがるSysExをcodec/helperが処理 |
| Event / Lifecycle | 接続、GATT完了、通知、Security、HIDなどの非同期event配送と`begin()` / `end()` | 非同期callbackは`ble.update()`を呼ぶloop taskから配送。同期応答が必要なGATT Serverの`onRead()`だけはstack task上で実行 |

API単位の対応状況と制限は[機能対応マトリクス](docs/FEATURE_MATRIX.ja.md)、
用途別のexampleは[examplesの目次](examples/README.ja.md)を参照してください。

上記の全機能はESP32-S3 2台の自動Peerテストとhost上のunit testで検証しています。
P4/C6 Hosted構成は接続、GATT、notify/indicate、MTU、Wi-Fi/BLE共存と共有transportの
lifecycleをP4/S3で実機確認済みです。Securityなどの対象外範囲を含む詳細は
[テスト計画](tests/TEST_PLAN.ja.md)と
[ESP-Hostedの既知制限](docs/ESP_HOSTED_LIMITATIONS.ja.md)を参照してください。

## 対応環境

EspBleは**Arduino-ESP32 CoreへESP-IDF componentとして組み込まれたNimBLE Host API**を
直接使用します。NimBLEを提供しない構成はコンパイル時に`#error`で拒否します。

無印`esp32`ボードだけは例外で、Arduino-ESP32のプリビルドがBluedroidであるため、
EspBleがNimBLE Host（`src/nimble_esp32/`、esp-idfがpinするesp-nimbleと同一スナップショット）
を同梱して動かします。設定値は他ターゲットと同一に固定し、利用者の上書きは拒否します。

**無印ESP32では兄弟ライブラリ[EspBleBluedroid](https://github.com/tanakamasayuki/EspBleBluedroid)の
利用を推奨します。** EspBle側は「NimBLEでも動かせる特殊対応」で、hostの保守を自前で負う、
Bluetooth Classic（SPP等）と同居できない、という不利があります。加えて無印ESP32は
BLE 4.2 controllerのため**LE 2M / Coded PHYが使えず**、Extended / Periodic Advertisingも
使えません。同時接続数の上限は3です。実機Peerテストで確認できた範囲
（GATT read/write/discovery、MTU、接続パラメータ更新、pairing・bonding、HID Device、
HID Host、BLE MIDI Device、Central / Peripheral両役割）のみを対応済みとし、
タイミング依存の挙動が他ターゲットと一致することは保証しません。詳細と検証記録は
[無印ESP32対応計画](docs/PLAN_ESP32.ja.md)にあります。

ESP32-P4はArduino-ESP32が提供するESP-Hosted NimBLE構成で利用できる。検証済みの
Host/Slave version、C6更新方法、対応済み範囲は
[ESP-Hostedセットアップ](docs/ESP_HOSTED_SETUP.ja.md)を参照する。Core 3.3.11では
同梱IDFのP4 ECC不具合によりLE Secure Connections、bonding、それを前提とするHID、
および`end()`後の複数回の再`begin()`には
[既知制限](docs/ESP_HOSTED_LIMITATIONS.ja.md)がある。
Tab5や独自基板でSDIO pin配置がgeneric P4と異なる場合は、正しいboard variantを
選ぶか、初期化前にCoreのpin設定を上書きする。手順と実行可能なsketchは
[SDIO pinの選択と上書き](docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き)と
[Hosted/CustomPins](examples/Hosted/CustomPins/)を参照する。

開発とPeerテストはarduino-esp32 3.3.11で行っています。対応するcoreバージョンの範囲とボードごとのビルドカバレッジは手動管理ではなくCIで計測します:

- **Core Compatibility Matrix** ワークフロー → `docs/COMPATIBILITY.<version>.md`（S3 / C3 / C6 / H2 / P4の代表exampleをarduino-esp32の各リリースに対してビルド）
- **Board Build Coverage** ワークフロー → `docs/BOARDS.<version>.md`（1つのcoreバージョンで全exampleをESP32-S3 / ESP32 / C3 / C6 / H2 / P4に対してビルド）

どちらもフルsweepが全sketchを書き換えて再ビルドするため、手動実行（`workflow_dispatch`）です。確定した最小coreバージョンは生成されたマトリクスを参照してください。

## はじめかた

Arduino Library Managerで`EspBle`を検索するか、Arduino CLIでinstallします。

```sh
arduino-cli lib install EspBle
```

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
