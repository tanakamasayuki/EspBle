# 開発状況

> English: [STATUS.md](STATUS.md)

この文書は現在の実装状況、既知の制限、1.0.0までの残作業だけを追跡します。対応機能の一覧は[FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)、確定仕様は[REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)・[DECISIONS.ja.md](DECISIONS.ja.md)・各仕様書を正とします。

## 現在地

Arduino-ESP32 3.3.11に同梱されるNimBLEホストAPIを直接使い、Central / Peripheral、GATT Client / Server、Security、複合HID Device / Hostが動作しています。**同梱の`BLE`ラッパクラス（`BLEDevice` / `BLEClient` / `BLEServer` / `BLEScan` / `BLEAdvertising` など）には依存していません**（理由は[DECISIONS.ja.md](DECISIONS.ja.md)の「アーキテクチャで確定」）。ESP32-S3 2台を使うPeerテスト環境とhost unit testがあり、公開exampleはESP32-S3でコンパイル検証されています。

ESP32-P4 + ESP32-C6のESP-Hosted構成は、compile、scan、接続、GATT read/write、
notify/indicate、MTU、Wi-Fi/BLE共存と共有transport lifecycleに加え、Security非依存の
Peer test 23件をP4/S3で実機確認済みです。
Security/bondingはCore 3.3.11同梱IDFのP4 ECC不具合、複数回の完全再初期化は
Hosted 2.12.11のresource leakにより、
[ESP-Hostedの既知制限](ESP_HOSTED_LIMITATIONS.ja.md)として上流修正取り込み待ちです。

HIDはKeyboard（6KRO / NKRO）、Mouse、Consumer Control、System Control、Gamepad、Vendor Input / Output / Featureを1つのServiceへ合成できます。Hostは対応する全Input ReportをDiscoveryし、種別別eventへ配送します。

BLE MIDIはbackend非依存のpacket codec（timestamp・running status・複数パケットSysEx）と、USB姉妹ライブラリに合わせた`EspBleMidiDevice` / `EspBleMidiHost` profile helperを提供します。

## 検証状況

- P4/C6 Hosted + S3 Peer: Security非依存の追加23 testが成功（GAP/controller 9、advertising 6、GATT state 8）
- P4/C6 Hosted Wi-Fi/BLE共存: DHCP取得中も含むWi-Fi接続、GATT read/write/notify、BLE終了後のWi-Fi維持、Wi-Fi終了後のtransport解放が成功

- Peer test: 66 suite、88 test。接続、GATT、接続ごとdiscovery cache、persistent subscription（再接続時に自動で再購読、registry上限超過の計数）、address privacy（random static address）、iBeacon broadcast/decode、Service Data送受信、Fitness Machine（Indoor Bike Data）、Security、標準Service、複合HID、NKRO、任意Report DescriptorのCustom HID、non-connectable Beacon、BLE MIDI、Health Thermometer、Blood Pressure、Weight Scale、Body Composition、Cycling / Running Speed and Cadence、Cycling Power、Pulse Oximeter、Glucose（RACP手続き）、Location and Navigation、User Data（書き込み→onWritten→notify）、Alert Notification（Control Point→notify）、Immediate Alert（Write Without Response）、Phone Alert Status（Control Point→状態変更notify）、Proximity（Link Loss + Tx Power、2 Service同居）、Reference Time Update（Control Point→state遷移）、Bond Management（Feature Read + Control Point）、Continuous Glucose Monitoring（E2E-CRC）、切断理由コード、接続パラメータ更新、PHY更新（2M）、Service Changed、実行時passkey入力、Numeric Comparison、Directed Advertising（チャネル絞り込み含む）、切断時のGATT queue purgeとGATT op中`disconnect()`の遅延実行、NKROのMTU下限拒否、多重listenerの配送・解除・上限（generic側・接続系・HID Host側）、接続系listenerのprimary→登録順の配送、HID Deviceの`ready()`が購読gateどおりに真偽を返すこと、NKRO全状態の1 report送信、`heldState()`が全送信経路に追従すること、`ledState()`がHostの書いたLEDを返すこと（callback未設定でqueueが溢れている間も追従すること）、HIDの便利入力API（`pressKey()` / `tapKey()` / `write()` / `tapUsage()` / `setLayout()` / mouse `wheel()` / `click()` / consumer・system `sendUsage()` / gamepad `send()`）が電波へ出すReport、Custom HIDのFeature Report、HID Boot Protocol切替、Custom HID Report Descriptor、non-connectable Beacon（送信間隔制御）、異常系、再接続を実機検証
- Manual test（3台目board前提、未接続時は自動skip）: `multi_connection`で複数同時接続・接続ごとのnotify routing・auto-reconnect（`setAutoReconnect`）・再接続時のpersistent subscription復元を実機検証
- Unit test: keymap変換、HID Report Map parser、BLE MIDI codec、IEEE-11073 medical float codec、CGM E2E-CRC codec、iBeacon codec、HCI router、HCI command scheduler
- 追加RPA Peer test: 無印ESP32 2台でhost-based RPAの初回pairing、両端再起動後のbond/LTK復元、暗号化GATT、復元bond削除、再pairingが成功。さらにClassic HID ACLを維持したまま両roleのRPAをhost timerでrotationし、preemptされたadvertisingとscanを再開して新RPAからbond済みLEへ再接続する。両dual-host stackの再起動後にもClassicとbond済みRPA/LEを復元する試験が成功。上記集計と合わせて68 suite、90 test
- Example compile: ESP32-S3向け92 example

実行方法は[tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md)、リリース時の確認項目は[RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md)を参照してください。

## 既知の制限

- 1.0.0リリース前のため、公開APIは互換性を保証しません。
- Core 3.3.11のP4/C6 ESP-Hosted構成では、同梱ESP-IDF 5.5.5のTinyCrypt/ECC不具合で
  LE Secure ConnectionsがDHKey check failureとなるため、Security、bonding、それを
  前提とするHIDは未対応です。ESP-IDF `release/v5.5`では`9fd7cb7`で修正済みですが、
  Arduino Coreへの取り込み待ちです。またArduino-ESP32 3.3.11の
  Hosted 2.12.11では複数回の完全deinit/initでSDIO mempool確保に失敗するため、
  `begin()`は起動後1回とし、再開にはP4を再起動します。詳細は
  [ESP-Hostedの既知制限](ESP_HOSTED_LIMITATIONS.ja.md)を参照してください。
- BLE MIDIのSysEx送信は1メッセージ320 byteまでです（送受信ともに複数BLEパケットへ分割・再構成します）。同時に進行できるSysEx送信は1件です。
- Gamepad Hostはvariable input fieldを解析しますが、vendor固有array inputの意味解釈は行いません。
- HID Hostは接続ごとに明示的な`discover(connectionId)`が必要です。Security有効時はSecurity完了後に呼びます。アプリの`onSecurityChanged`を奪わずに済ませたい統合層は、`addSecurityChangedListener()`から呼べます。一度discoveryしたpeerの再接続は`setAutoRediscover(true)`で自動化できます（既定off）。
- 任意のReport DescriptorのCustom HIDは`ble.hidCustom()`で構成できます（`setReportMap()`＋`addInputReport()` / `addOutputReport()` / `addFeatureReport()`）。カスタムReportは内蔵profileと同じHID Serviceへ合成され共存できます。Report IDは一意で、内蔵profile併用時はその予約ID（1〜6）を避けます。1デバイスあたりカスタムReportは最大4件です。
- HID KeyboardはBoot Protocol（Protocol Mode characteristic 0x2A4E、Boot Keyboard Input/Output Report 0x2A22/0x2A32）に対応しますが、`EspBleHidKeyboardConfig::bootProtocol`によるopt-in（既定off）です。多くのHOGP HostはReport Protocol Modeで足り、追加characteristicはすべてのHostのDiscoveryを増やす（後述のupstream discoveryリークを増幅する）ため既定offとしています。有効時はBoot Protocol Modeで入力が自動的に8 byteのBoot Keyboard Input Reportへ切り替わり、`onProtocolMode()` / `protocolMode()`でモードを確認できます。Boot Protocolは現状Keyboardのみで、Mouse Boot Report（0x2A33）は未対応です。
- Central側GATT operationは実際のATT送受信は同時1件ですが、`readCharacteristic()`等の呼び出しは**自動でキューへ積まれ順に実行**されます（「operation already in progress」で失敗しません）。operationごとの強制cancelはありません。**キューは実行中1件のほかに8件まで**で、それを超える呼び出しは`ResourceExhausted`でfalseを返します（電波に出る前に拒否されるので、失敗した購読はpersistent subscriptionのレコードにも残りません）。
- GATT ClientのRead/Write/Subscribe/Unsubscribeと、Descriptorのread/writeはUUID指定に加えて**attribute handle指定のオーバーロード**があります。UUIDが重複するcharacteristic（例: HID Serviceの複数Report 0x2A4D）は、`discoverServices()`後に`discoveredCharacteristic()`でhandleを取得しhandleで撃ち分けます。`EspBleGattResult` / `EspBleGattNotification` は対象の`handle`を保持します。Descriptorも`readDescriptor(id, descriptorHandle)` / `writeDescriptor(id, descriptorHandle, ...)`でhandle指定でき、`discoveredDescriptor()`の`characteristicHandle`で持ち主を辿れます。**HIDのReport Reference（0x2908）のように、UUIDが重複するcharacteristicに属するdescriptorはこの形でしか指定できません。** 結果の`descriptorHandle`が対象のdescriptor、`handle`がそれを持つcharacteristicです。Discoveryはhandle単位で列挙するため、同一UUIDのcharacteristicも個別に列挙されます。**同一UUIDのServiceを複数公開する相手にも到達できます**（discoveryを`ble_gattc_disc_all_svcs()`直呼びで行い、read / write / CCCD書き込みも属性ハンドルへ直接発行するため）。ただし再接続時の購読自動復元はpeerアドレスとUUIDで引くので、UUIDが一意なCharacteristicに限ります。
- 汎用GATT Clientのdiscoveryを繰り返してもヒープは減りません（`gatt_read_write` の `test_discovery_cycles_do_not_leak_heap` が8サイクルで計測します）。以前観測していた1サイクルあたり約1.2 KBの減少は、GATT操作用のFreeRTOSタスクが `vTaskDelete(nullptr)` で終了してローカル変数のデストラクタが走らず、`String` のヒープバッファを取り残していた自前の不具合でした。タスク本体を別関数へ分けて解消しています。
- 同梱の`BLE`ラッパを使うと、GATT client discoveryの反復で解放されないヒープリークがあります（discoveryしたcharacteristic数に比例、約2.6 KB/discovery）。**EspBleはこの経路を持ちません**——generic client・HID Host・MIDI Hostのdiscoveryはいずれもラッパのremoteオブジェクトを作らず、NimBLEホストAPIへ直接発行します。`gatt_read_write`の`test_discovery_cycles_do_not_leak_heap`が8サイクルで実測します。
- GATT Client Discovery snapshotは接続ごとに保持します（接続数上限まで、切断で解放）。購読は`EspBleConfig::persistentSubscriptions`（既定on）で同一peerへの再接続時に自動復元します。Service Changed indicationはServer側`notifyServicesChanged()`で送出、Client側は0x1801/0x2A05を購読して受信・decodeできますが、受信時の自動再Discoveryは行いません（アプリが再discoverを判断します）。
- 切断理由は`EspBleConnection::disconnectReason`、接続パラメータは`EspBleConnection`のinterval/latency/timeoutと`updateConnectionParameters()` / `onConnectionParametersUpdated()`、LE PHYは`EspBleConnection`のtx/rxPhyと`updatePhy()` / `onPhyUpdated()`、実行時passkey入力は`providePasskey()`（動的passkey表示は静的passkeyなしのDisplayOnly）、Numeric Comparisonは両側DisplayYesNo + MITMで`onNumericComparison()` / `confirmNumericComparison()`で扱えます。
- Descriptor Write eventは`EspBleGattDescriptorWrite::connectionId`でどの接続からの書き込みかを持ちます（属性テーブルをNimBLEホストAPIで自前に組んでおり、ホストのaccess callbackが`conn_handle`を渡すため）。ただしこのフィールドはPeerテストでassertしていません。
- MTU交換はグローバルGAPイベント（`BLE_GAP_EVENT_MTU`）で両役割とも追跡し、`onMtuChanged`へ配送します。Central側で接続確立後に完了するMTU交換も反映されます。
- Advertisingはconnectable（既定）とnon-connectable（`setConnectable(false)`。Beacon/broadcaster）を選べ、`setScanResponseEnabled(false)`でnon-scannable、`setInterval(minMs, maxMs)`で送信間隔（20〜10240 ms、non-connectableは100 ms以上）を制御できます。Address privacyは`EspBleConfig::ownAddressType`（`Public`（既定） / `RandomStatic` / `ResolvablePrivate`）で選べます。RandomStaticはpublic addressを隠す固定random static address、ResolvablePrivateはcontrollerまたは無印ESP32の同梱hostが周期回転するRPA（既定900秒）で、RPAはpeerがbonding時のIRKで解決するためsecurity/bonding併用時のみ有用です。Extended / Periodic Advertisingは同梱NimBLEが`CONFIG_BT_NIMBLE_EXT_ADV`無効でビルドされているため現構成では対応不可です。
- 同時複数接続に対応します（接続ごとのcache・購読・GATT routingで分離）。同時接続数の上限は同梱NimBLE controller（`CONFIG_BT_NIMBLE_MAX_CONNECTIONS`、esp32s3で3）で決まります。auto-reconnect（`setAutoReconnect`、既定off）と併せて3台manual test `multi_connection`で検証済みです。
- 自動実機検証はESP32-S3中心です。市販機器およびAndroid / Linux / Windows / macOSとの相互運用確認は未完了です。
- 外部NimBLE-Arduinoは対象外です。無印ESP32はEspBleがNimBLE Hostを同梱して動かします（`src/nimble_esp32/`）。加えて独自buildしたClassic-only Bluedroid hostによるSPPとgeneric HID Device/Hostを実験対応しています。排他構成に加え、`ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL`ではClassicを先にBTDMで起動し、Classic HID接続中のLE接続、GATT read 25回、その後のHID双方向通信まで実機確認済みです。共有HCI command schedulerはbroker所有FIFO、controller command credit、opcode応答照合を実装し、完全再ビルドの同負荷試験でqueue overflow・応答不一致・取りこぼし0を確認しました。General / Page 2 / LE event maskはhost別要求をbrokerでunionし、Classic再attach時のHCI Resetとflow-control設定は仮想完了します。Classic HID接続中のBLE pairing、bond保存・再接続、暗号化必須GATT readも両側で成功しました。host-based RPAはadvertising/scanのprivacy preemption後に元条件と有限deadlineを維持して再開し、10周期連続のRPA更新とClassic HID通信、LE Set Random AddressのNimBLE routing、broker error 0を両基板で確認済みです。controller停止責任もbrokerへ委譲し、NimBLE→Classic、Classic→NimBLEの両停止順、Classic停止後のGATT継続、BLE接続中のClassic再attachとHID再接続、同一instanceによる再登録・正常停止20サイクル、両順序の実destructorを両側で確認済みです。ただし長時間再登録soak、未分類のcontroller-wide command、長時間負荷試験は未完了なので一般対応とはしません。NimBLEはソース、Classicは`.a`という現在の同梱形式は、機能安定後にどちらかへ統一する将来課題として[Classic計画](PLAN_ESP32_CLASSIC.ja.md#将来の配布形式統一)へ記録しています。HIDは任意Report Descriptor、Input/Output Report、接続・切断・再初期化をESP32 2台のPeer testで検証済みです。実機Peerテストで確認できた範囲だけを対応済みとします——方針・検証記録は[PLAN_ESP32.ja.md](PLAN_ESP32.ja.md)と[PLAN_ESP32_CLASSIC.ja.md](PLAN_ESP32_CLASSIC.ja.md)にあります。

- dual-host再登録の100サイクル試験では、baseline、各サイクル、destructor後の計102点で両基板のfree heapとlargest blockがそれぞれ同一で、観測された減少は0 byteでした。数時間級のsoakは未検証です。
- dual-host HCI command競合は、別taskのClassic scan mode切替とNimBLE Read RSSIを10サイクル反復し、各基板で競合区間だけで約210件／約500件を送信しました。最大queue深度4／5、投入＝物理送信、broker error 0で、各サイクル後の暗号化GATTとHID双方向通信、全lifecycle、heap安定性まで成功しています。test-only dispatch holdでは両hostから24件を同時投入し、16 packet FIFO満杯、8件の超過拒否、未送信分破棄後のlive session復帰も両基板で確認しました。数時間級soakは未検証です。`Write Local Name`の反復はcontrollerのNVDS assertionになるため、負荷刺激から除外しています。

## 1.0.0までの残作業

1. 全Peer + unit testを`--clean`で連続実行し、複数回反復する。
2. board / Arduino-ESP32 core matrixをCIで再生成し、対応環境を確定する。
3. 市販BLE Keyboardと複数の外部Hostでmanual interoperabilityを確認する。
4. metadata、CHANGELOG、example、仕様書を最終APIと照合する。
5. `library.properties`を含むrelease metadataを1.0.0へ更新し、release workflowを実行する。

残作業の一覧は[PLAN_RELEASE_1_0_0.ja.md](PLAN_RELEASE_1_0_0.ja.md)が正本です。初回リリース範囲は固定せず、安全に実装・検証できる機能は1.0.0へ含めます。未実装候補は約束ではなく、採用時に仕様、example、unit/build/Peer testを同時に追加します。今後の機能候補は[DECISIONS.ja.md](DECISIONS.ja.md)の「優先順位候補」を正とします。

## 更新ルール

- 完了機能の詳細な列挙はFeature Matrixと仕様書へ記載し、この文書へ重複させません。
- 実装だけで完了にせず、対応するexampleとunit/build/Peer/manual testの状況も更新します。
- 過去の計画や完了チェックリストは残さず、設計上の理由だけを`DECISIONS.ja.md`へ残します。
