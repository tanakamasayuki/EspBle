# 開発状況

最終更新: 2026-07-21

この文書は現在の実装状況、既知の制限、1.0.0までの残作業だけを追跡します。対応機能の一覧は[FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)、確定仕様は[REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)・[DECISIONS.ja.md](DECISIONS.ja.md)・各仕様書を正とします。

## 現在地

Arduino-ESP32 3.3.10のNimBLE backendを使い、Central / Peripheral、GATT Client / Server、Security、複合HID Device / Hostが動作しています。ESP32-S3 2台を使うPeerテスト環境とhost unit testがあり、公開exampleはESP32-S3でコンパイル検証されています。

HIDはKeyboard（6KRO / NKRO）、Mouse、Consumer Control、System Control、Gamepad、Vendor Input / Output / Featureを1つのServiceへ合成できます。Hostは対応する全Input ReportをDiscoveryし、種別別eventへ配送します。

BLE MIDIはbackend非依存のpacket codec（timestamp・running status・複数パケットSysEx）と、USB姉妹ライブラリに合わせた`EspBleMidiDevice` / `EspBleMidiHost` profile helperを提供します。

## 検証状況

- Peer test: 24 suite、35 test。接続、GATT、Security、標準Service、複合HID、NKRO、BLE MIDI、異常系、再接続を実機検証
- Unit test: keymap変換、HID Report Map parser、BLE MIDI codec
- Example compile: ESP32-S3向け38 example
- ESP32KeyBridge試作adapter: raw usage、remap、modifier、切断release、LED返送、Bond再接続をPeer検証

実行方法は[tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md)、リリース時の確認項目は[RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md)を参照してください。

## 既知の制限

- 1.0.0リリース前のため、公開APIは互換性を保証しません。
- HID KeyboardのBoot Protocol characteristic / Protocol Mode切替は未対応です。
- Custom HIDは固定Vendor Report以外の任意Report Descriptorをまだ登録できません。
- BLE MIDIの送信は単一BLEパケットに収まるメッセージのみで、大きなSysExの複数パケット分割送信は未対応です（受信側parserは複数パケットSysExを再構成できます）。
- Gamepad Hostはvariable input fieldを解析しますが、vendor固有array inputの意味解釈は行いません。
- HID Hostは接続ごとに明示的な`discover(connectionId)`が必要です。Security有効時はSecurity完了後に呼びます。
- Central側GATT operationは同時1件です。operation queueと強制cancelはありません。
- GATT Client Discovery snapshotは最新1接続分で、永続cacheとService Changed追従はありません。
- 切断理由、接続パラメータ更新、実行時passkey、Numeric Comparisonは未対応です。
- Descriptor Write eventはbackendがconnection contextを公開しないためConnection IDを持ちません。詳細は[upstream依頼案](UPSTREAM_REQUEST_ARDUINO_ESP32_DESCRIPTOR_CONTEXT.ja.md)を参照してください。
- 接続後のMTU変更は追跡できず、接続時snapshotを使用します。
- 同時複数接続は接続単位APIを維持していますが、自動試験と公開動作保証の対象外です。
- 自動実機検証はESP32-S3中心です。市販機器およびAndroid / Linux / Windows / macOSとの相互運用確認は未完了です。
- Bluedroid backend、Bluetooth Classic、外部NimBLE-Arduinoは対象外です。

## 1.0.0までの残作業

1. `FEATURE_MATRIX.ja.md`のうち1.0.0へ含めるHID拡張を可能な範囲で実装する。
2. 全Peer + unit testを`--clean`で連続実行し、複数回反復する。
3. board / Arduino-ESP32 core matrixをCIで再生成し、対応環境を確定する。
4. 市販BLE Keyboardと複数の外部Hostでmanual interoperabilityを確認する。
5. metadata、CHANGELOG、example、仕様書を最終APIと照合する。
6. `library.properties`を含むrelease metadataを1.0.0へ更新し、release workflowを実行する。

初回リリース範囲は固定せず、安全に実装・検証できる機能は1.0.0へ含めます。未実装候補は約束ではなく、採用時に仕様、example、unit/build/Peer testを同時に追加します。

## 次の機能候補

1. Custom HID Report Descriptor
2. HID Boot Protocol切替
3. 実行時passkey / Numeric Comparison
4. reconnect cache / resubscribe / multiple connections
5. Sensor profile、大きなSysExの複数パケット送信
6. Extended / Periodic Advertising、PHY、Privacy、Beacon

## 更新ルール

- 完了機能の詳細な列挙はFeature Matrixと仕様書へ記載し、この文書へ重複させません。
- 実装だけで完了にせず、対応するexampleとunit/build/Peer/manual testの状況も更新します。
- 過去の計画や完了チェックリストは残さず、設計上の理由だけを`DECISIONS.ja.md`へ残します。
