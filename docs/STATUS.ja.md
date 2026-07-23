# 開発状況

この文書は現在の実装状況、既知の制限、1.0.0までの残作業だけを追跡します。対応機能の一覧は[FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)、確定仕様は[REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)・[DECISIONS.ja.md](DECISIONS.ja.md)・各仕様書を正とします。

## 現在地

Arduino-ESP32 3.3.11のNimBLE backendを使い、Central / Peripheral、GATT Client / Server、Security、複合HID Device / Hostが動作しています。ESP32-S3 2台を使うPeerテスト環境とhost unit testがあり、公開exampleはESP32-S3でコンパイル検証されています。

HIDはKeyboard（6KRO / NKRO）、Mouse、Consumer Control、System Control、Gamepad、Vendor Input / Output / Featureを1つのServiceへ合成できます。Hostは対応する全Input ReportをDiscoveryし、種別別eventへ配送します。

BLE MIDIはbackend非依存のpacket codec（timestamp・running status・複数パケットSysEx）と、USB姉妹ライブラリに合わせた`EspBleMidiDevice` / `EspBleMidiHost` profile helperを提供します。

## 検証状況

- Peer test: 56 suite、67 test。接続、GATT、接続ごとdiscovery cache、persistent subscription（再接続時に自動で再購読）、address privacy（random static address）、iBeacon broadcast/decode、Service Data送受信、Fitness Machine（Indoor Bike Data）、Security、標準Service、複合HID、NKRO、任意Report DescriptorのCustom HID、non-connectable Beacon、BLE MIDI、Health Thermometer、Blood Pressure、Weight Scale、Body Composition、Cycling / Running Speed and Cadence、Cycling Power、Pulse Oximeter、Glucose（RACP手続き）、Location and Navigation、User Data（書き込み→onWritten→notify）、Alert Notification（Control Point→notify）、Immediate Alert（Write Without Response）、Phone Alert Status（Control Point→状態変更notify）、Proximity（Link Loss + Tx Power、2 Service同居）、Reference Time Update（Control Point→state遷移）、Bond Management（Feature Read + Control Point）、Continuous Glucose Monitoring（E2E-CRC）、切断理由コード、接続パラメータ更新、PHY更新（2M）、Service Changed、実行時passkey入力、Numeric Comparison、HID Boot Protocol切替、Custom HID Report Descriptor、non-connectable Beacon（送信間隔制御）、異常系、再接続を実機検証
- Manual test（3台目board前提、未接続時は自動skip）: `multi_connection`で複数同時接続・接続ごとのnotify routing・auto-reconnect（`setAutoReconnect`）・再接続時のpersistent subscription復元を実機検証
- Unit test: keymap変換、HID Report Map parser、BLE MIDI codec、IEEE-11073 medical float codec、CGM E2E-CRC codec、iBeacon codec
- Example compile: ESP32-S3向け82 example
- ESP32KeyBridge試作adapter: raw usage、remap、modifier、切断release、LED返送、Bond再接続をPeer検証

実行方法は[tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md)、リリース時の確認項目は[RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md)を参照してください。

## 既知の制限

- 1.0.0リリース前のため、公開APIは互換性を保証しません。
- BLE MIDIのSysEx送信は1メッセージ320 byteまでです（送受信ともに複数BLEパケットへ分割・再構成します）。同時に進行できるSysEx送信は1件です。
- Gamepad Hostはvariable input fieldを解析しますが、vendor固有array inputの意味解釈は行いません。
- HID Hostは接続ごとに明示的な`discover(connectionId)`が必要です。Security有効時はSecurity完了後に呼びます。
- 任意のReport DescriptorのCustom HIDは`ble.hidCustom()`で構成できます（`setReportMap()`＋`addInputReport()` / `addOutputReport()` / `addFeatureReport()`）。カスタムReportは内蔵profileと同じHID Serviceへ合成され共存できます。Report IDは一意で、内蔵profile併用時はその予約ID（1〜6）を避けます。1デバイスあたりカスタムReportは最大4件です。
- HID KeyboardはBoot Protocol（Protocol Mode characteristic 0x2A4E、Boot Keyboard Input/Output Report 0x2A22/0x2A32）に対応しますが、`EspBleHidKeyboardConfig::bootProtocol`によるopt-in（既定off）です。多くのHOGP HostはReport Protocol Modeで足り、追加characteristicはすべてのHostのDiscoveryを増やす（後述のupstream discoveryリークを増幅する）ため既定offとしています。有効時はBoot Protocol Modeで入力が自動的に8 byteのBoot Keyboard Input Reportへ切り替わり、`onProtocolMode()` / `protocolMode()`でモードを確認できます。Boot Protocolは現状Keyboardのみで、Mouse Boot Report（0x2A33）は未対応です。
- Central側GATT operationは実際のATT送受信は同時1件ですが、`readCharacteristic()`等の呼び出しは**自動でキューへ積まれ順に実行**されます（「operation already in progress」で失敗しません）。operationごとの強制cancelはありません。
- GATT ClientのRead/Write/Subscribe/UnsubscribeはUUID指定に加えて**attribute handle指定のオーバーロード**があります。UUIDが重複するcharacteristic（例: HID Serviceの複数Report 0x2A4D）は、`discoverServices()`後に`discoveredCharacteristic()`でhandleを取得しhandleで撃ち分けます。`EspBleGattResult` / `EspBleGattNotification` は対象の`handle`を保持します。Discoveryはhandle単位で列挙するため、同一UUIDのcharacteristicも個別に列挙されます。
- 同梱NimBLE backendには、GATT client discoveryを反復すると解放されないヒープリーク（discoverしたcharacteristic数に比例、約2.6 KB/discovery）があります。多数peerへの順次接続や再接続の長時間反復で徐々にヒープが減少します。EspBle側では回避できず（backendが接続ごとに再discoveryを強制）、詳細と回避方針は[upstream報告案](UPSTREAM_REQUEST_ARDUINO_ESP32_GATTC_DISCOVERY_LEAK.ja.md)を参照してください。Boot Protocolを既定offにしたのはこのリークを不要に増幅しないためです。
- GATT Client Discovery snapshotは接続ごとに保持します（接続数上限まで、切断で解放）。購読は`EspBleConfig::persistentSubscriptions`（既定on）で同一peerへの再接続時に自動復元します。Service Changed indicationはServer側`notifyServicesChanged()`で送出、Client側は0x1801/0x2A05を購読して受信・decodeできますが、受信時の自動再Discoveryは行いません（アプリが再discoverを判断します）。
- 切断理由は`EspBleConnection::disconnectReason`、接続パラメータは`EspBleConnection`のinterval/latency/timeoutと`updateConnectionParameters()` / `onConnectionParametersUpdated()`、LE PHYは`EspBleConnection`のtx/rxPhyと`updatePhy()` / `onPhyUpdated()`、実行時passkey入力は`providePasskey()`（動的passkey表示は静的passkeyなしのDisplayOnly）、Numeric Comparisonは両側DisplayYesNo + MITMで`onNumericComparison()` / `confirmNumericComparison()`で扱えます。
- Descriptor Write eventはbackendがconnection contextを公開しないためConnection IDを持ちません。詳細は[upstream依頼案](UPSTREAM_REQUEST_ARDUINO_ESP32_DESCRIPTOR_CONTEXT.ja.md)を参照してください。
- MTU交換はグローバルGAPイベント（`BLE_GAP_EVENT_MTU`）で両役割とも追跡し、`onMtuChanged`へ配送します。Central側で接続確立後に完了するMTU交換も反映されます。
- Advertisingはconnectable（既定）とnon-connectable（`setConnectable(false)`。Beacon/broadcaster）を選べ、`setScanResponseEnabled(false)`でnon-scannable、`setInterval(minMs, maxMs)`で送信間隔（20〜10240 ms、non-connectableは100 ms以上）を制御できます。Address privacyは`EspBleConfig::ownAddressType`（`Public`（既定） / `RandomStatic` / `ResolvablePrivate`）で選べます。RandomStaticはpublic addressを隠す固定random static address、ResolvablePrivateはcontrollerが周期回転するRPA（`CONFIG_BT_NIMBLE_RPA_TIMEOUT`＝900秒）で、RPAはpeerがbonding時のIRKで解決するためsecurity/bonding併用時のみ有用です。Extended / Periodic Advertisingは同梱NimBLEが`CONFIG_BT_NIMBLE_EXT_ADV`無効でビルドされているため現構成では対応不可です。
- 同時複数接続に対応します（接続ごとのcache・購読・GATT routingで分離）。同時接続数の上限は同梱NimBLE controller（`CONFIG_BT_NIMBLE_MAX_CONNECTIONS`、esp32s3で3）で決まります。auto-reconnect（`setAutoReconnect`、既定off）と併せて3台manual test `multi_connection`で検証済みです。
- 自動実機検証はESP32-S3中心です。市販機器およびAndroid / Linux / Windows / macOSとの相互運用確認は未完了です。
- Bluedroid backend、Bluetooth Classic、外部NimBLE-Arduinoは対象外です。

## 1.0.0までの残作業

1. `FEATURE_MATRIX.ja.md`のうち1.0.0へ含めるHID拡張を可能な範囲で実装する。
2. 全Peer + unit testを`--clean`で連続実行し、複数回反復する。
3. board / Arduino-ESP32 core matrixをCIで再生成し、対応環境を確定する。
4. 市販BLE Keyboardと複数の外部Hostでmanual interoperabilityを確認する。
5. metadata、CHANGELOG、example、仕様書を最終APIと照合する。
6. `library.properties`を含むrelease metadataを1.0.0へ更新し、release workflowを実行する。

初回リリース範囲は固定せず、安全に実装・検証できる機能は1.0.0へ含めます。未実装候補は約束ではなく、採用時に仕様、example、unit/build/Peer testを同時に追加します。今後の機能候補は[DECISIONS.ja.md](DECISIONS.ja.md)の「優先順位候補」を正とします。

## 更新ルール

- 完了機能の詳細な列挙はFeature Matrixと仕様書へ記載し、この文書へ重複させません。
- 実装だけで完了にせず、対応するexampleとunit/build/Peer/manual testの状況も更新します。
- 過去の計画や完了チェックリストは残さず、設計上の理由だけを`DECISIONS.ja.md`へ残します。
