# テスト計画

## 方針

BLEは接続、切断、Discovery、購読、Security、Bondingが複数の非同期イベントにまたがります。このためPeerテストを補助的なsmokeではなく、実装を進めるための主要な自動テストにします。

- unit: keymap変換、HID Report Map parserなどをhost上のg++で検証する（`tests/unit/`）。
- examples_compile: 公開APIと対象SoCのbuild回帰を検出する。`.github/workflows/compile-examples.yml`が全exampleをesp32s3 profileでコンパイルする（push/PRで自動実行。カバレッジ表のbuild列✅はこの検証を指す）。
- peer: ESP32-S3 2台で実際のradio、controller、host stackを通した接続を検証する。
- manual: Android/iOS/Windows/Linux/macOSや市販機器との相互運用を検証する。

Peer不要のruntime behaviorを1台で検証する「single」層は現在使用していません。必要になった時点で追加します。

## Peerハードウェア

EspUsbHost/EspUsbDeviceなどで常時接続されているESP32-S3 2台を共用します。BLE通信のためのボード間配線は不要です。各ボードをPCへ接続するSerial/給電だけを使用します。

これに加えてmanual test用ESP32-S3が1台あります。BLEはボード間の有線接続を必要としないため、将来3台が必要なscenarioでは追加のPeerディレクトリとprofile/port設定を用意して、この1台を第3Peerとして利用できます。初期テストの必須環境は常設2台のままとし、3台構成は複数接続やBLE-to-BLE bridgeのE2E testを追加するときに使用します。

pytest-embedded-cliの既存規約に従います。

- 親側profile: `s3_peer_host`
- 2台目profile: `s3_peer_device`
- 2台目directory: `peer_device/`
- Python fixture: `peers["device"]`

これらの`host` / `device`はUSB roleでもBLE roleでもありません。pytest-embedded-cliは両方へsketchを転送して実行し、`dut`と`peers["device"]`の両Serialを観測・操作できます。

初期scenarioは親側sketchをCentral、2台目sketchをPeripheralに固定します。EspBle Centralを検証するときは親側の結果を主にassertし、EspBle Peripheralを検証するときはPeer側の結果を主にassertします。役割交換やコード配置の交換は前提にしません。

## Peerテスト原則

- テスト専用128-bit Service UUIDで周囲のBLE機器を除外する。
- device nameだけで接続相手を決めない。
- 可能な範囲で一方をArduino-ESP32同梱BLE APIの直接実装にする。
- 親CentralとPeer Peripheralの役割は固定したまま、EspBleを親側、Peer側、または両側へ組み込んで目的別に検証する。
- Serial logだけで合否をassertできるscenarioにする。
- 各テスト終了時にscan、advertising、subscription、connectionを停止する。
- Securityテストは開始時と終了時のBond/NVS状態を明示する。
- radio環境による一時的な遅延にtimeoutは許すが、無制限retryで不具合を隠さない。
- 接続・切断理由、MTU、Security状態を可能な限り両側で照合する。

## 3台Peerへの拡張

次のscenarioは3台構成の候補です。

- 1台のCentralから2台のPeripheralへの同時接続
- 2台のCentralから1台のPeripheralへの接続
- 接続ごとのsubscription、Security、Bond状態の分離
- BLE HID入力Peripheral → Bridge DUT（Central + Peripheral）→ 出力確認Central
- 一方の接続切断や再接続が他方へ影響しないこと

pytest-embedded-cliでは親側と既存`peer_device/`に加えてPeerディレクトリを追加し、それぞれのsketchとSerialを同じテストから操作・assertする構成にします。具体的なディレクトリ名と環境変数は最初の3台scenario追加時に、pytest-embedded-cliの命名規約に合わせて確定します。

## カバレッジ計画

| 領域 | unit | build | peer | manual |
|---|---|---|---|---|
| test fixture / bundled BLE stack | | ✅ | `stack_smoke` | |
| Advertising / Scan parser | 予定 | ✅ | ✅ `advertise_scan` / `advertise_payload`（raw AD構造） | generic scanner |
| connect / disconnect / timeout | 状態遷移 | ✅ | ✅ `connect_disconnect`（address直接再接続） / `lifecycle_stress`（接続timeoutの非同期失敗） | |
| 接続lifecycle / event queue / leak | | ✅ | ✅ `lifecycle_stress`（flood中切断、再接続heap、GATT中`end()`、connect中`end()`、scanner flush、GATT操作排他） | |
| GATT一覧/既知UUID discovery、Characteristic/Descriptor read/write | codec | ✅ | ✅ `gatt_read_write`（Server Descriptor event、操作timeout・遅延完了抑止を含む） | generic GATT app |
| notify / indicate / unsubscribe | queue | ✅ | ✅ `notify_indicate` | generic GATT app |
| MTU | validation | ✅ | ✅ `mtu` | |
| Pairing / Bonding | error/state | ✅ | ✅ `security_bond` | Android/Linux |
| static passkey / MITM | validation | ✅ | ✅ `security_passkey` | Android/Linux |
| encrypted characteristic | permission | ✅ | ✅ `security_bond` | |
| authenticated characteristic | permission | ✅ | ✅ `security_passkey` | |
| HID over GATT security | | ✅ | ✅ `hid_security`（未暗号化linkの拒否） | OS |
| reconnect / peer loss | state | ✅ | ✅ Bond再接続 / `lifecycle_stress`（radio消失をsupervision timeoutで検出） | |
| HID Keyboard Device | report codec（予定） | ✅ | ✅ `hid_keyboard_device` / `hid_robustness`（購読gate、queue満杯） | OS、市販HID Host |
| HID NKRO Device / Host | ✅ `unit/report_map` | ✅ | ✅ `hid_keyboard_nkro`（8キー、高usage、個別release、LED） | OS、市販HID Host |
| HID LED output | report codec（予定） | ✅ | ✅ `hid_keyboard_device` / `hid_keyboard_host`（WWR非block） | OS |
| Battery Service Server / Client | 1-byte codec | ✅ | ✅ HIDへの組込み / `battery_service` standalone | generic GATT app |
| Device Information Service | PnP ID 7-byte codec | ✅ | ✅ HIDへの組込み / `device_information` standalone | generic GATT app |
| Current Time Service | Current Time 10-byte codec | ✅ | ✅ `current_time` Read / Notify | generic GATT app |
| Heart Rate Service | flags / variable-length codec | ✅ | ✅ `heart_rate` Location Read / Measurement Notify | generic GATT app |
| Environmental Sensing Service | signed / scaled integer codec | ✅ | ✅ `environmental_sensing` Read / Temperature Notify | generic GATT app |
| BLE MIDI Device / Host | ✅ `unit/midi`（timestamp、running status、SysEx、builder、複数パケットencoder） | ✅ | ✅ `midi_device`（wire形式＋複数パケットSysExを同梱API Centralで検証） / `midi_host`（running status decode＋複数パケットSysEx送信を同梱API Peripheralで検証） | 市販BLE MIDI楽器 / DAW |
| Health Thermometer Service | ✅ `unit/medical_float`（IEEE-11073 FLOAT/SFLOAT） | ✅ | ✅ `health_thermometer`（Type Read＋FLOAT Measurement Indicate/decode） | generic GATT app / 市販体温計 |
| Blood Pressure Service | ✅ `unit/medical_float`（SFLOAT） | ✅ | ✅ `blood_pressure`（Feature Read＋SFLOAT Measurement Indicate/decode） | generic GATT app / 市販血圧計 |
| Weight Scale Service | 固定分解能uint16 | ✅ | ✅ `weight_scale`（Feature Read＋uint16 Measurement Indicate/decode） | generic GATT app / 市販体重計 |
| Body Composition Service | uint16 flags＋任意フィールド | ✅ | ✅ `body_composition`（Feature Read＋Body Fat Percentage/Weight Measurement Indicate/decode） | generic GATT app / 市販体組成計 |
| Cycling Speed and Cadence Service | 多フィールド整数レイアウト | ✅ | ✅ `cycling_speed_cadence`（Location Read＋多フィールドMeasurement Notify/decode） | generic GATT app / 市販CSCセンサー |
| Running Speed and Cadence Service | 混在幅整数レイアウト | ✅ | ✅ `running_speed_cadence`（Location Read＋混在幅Measurement Notify/decode） | generic GATT app / 市販RSCセンサー |
| Cycling Power Service | 符号付き16bit + 16bit flags | ✅ | ✅ `cycling_power`（Location Read＋符号付きpower Measurement Notify/decode） | generic GATT app / 市販パワーメーター |
| Pulse Oximeter Service（PLX） | SFLOAT | ✅ | ✅ `pulse_oximeter`（Features Read＋SFLOAT Spot-Check Indicate/decode） | generic GATT app / 市販パルスオキシメーター |
| Glucose Service（RACP手続き） | SFLOAT / date_time | ✅ | ✅ `glucose`（RACP write→Measurement notify→RACP応答indicate） | generic GATT app / 市販血糖値計 |
| Location and Navigation Service | flags駆動可変長 + sint32 | ✅ | ✅ `location_navigation`（LN Feature Read＋Location and Speed Notify/decode） | generic GATT app / 市販GPSセンサー |
| User Data Service | 書き込み可能char + notify | ✅ | ✅ `user_data`（Age/First Name write→onWritten→Database Change Increment notify、再read） | generic GATT app |
| Alert Notification Service | bitmask read + Control Point write + notify | ✅ | ✅ `alert_notification`（category bitmask Read＋Control Point write→New Alert notify/decode） | generic GATT app / 市販通知機器 |
| Immediate Alert Service（Find Me） | Write Without Responseのみ | ✅ | ✅ `immediate_alert`（Alert Level Write Without Response→onWritten） | generic GATT app / 市販Find Meタグ |
| HID Keyboard Host | ✅ `unit/report_map`（Vendor含む） | ✅ | ✅ `hid_keyboard_host`（全6 profile、Vendor双方向） / `hid_boot_keyboard`（Report IDなし、長さ異常） / `hid_robustness`（rollover、Discovery中disconnect拒否） | 市販keyboard |
| HID keyboard event / layout | ✅ `unit/keymap`（Unicode 4-plane、AltGr、CapsLock） | ✅ | ✅ EN-US / JA-JP / en-GB / de-DE / fr-FR、modifier | 残りの各言語実機 |
| ESP32KeyBridge input adapter | bridge core | ✅ | ✅ `ble_keybridge_keyboard`（remap / modifier / release / LED / reconnect / listener共存） | BLE-to-USB E2E |
| Central+Peripheral同時動作 | state | 予定 | 予定 | |

## 実装済みscenario

1. ✅ `stack_smoke`: 同梱NimBLE backendのBLE APIで2台接続、read/writeと双方のSerialを確認する。
2. ✅ `advertise_scan`: EspBle Advertising builderとScanner parser。
3. ✅ `connect_disconnect`: Connection identity、local role、接続と切断のloop context。
4. ✅ `gatt_read_write`: 汎用GATT Server/Client、一覧/既知UUID Discovery、応答あり/なしWrite、Descriptor Read/Write、操作timeout。
5. ✅ `notify_indicate`: subscription、unsubscribe、CCCD、送信結果、受信event queue。
6. ✅ `mtu`: 接続時MTU交換、両側snapshot/callback、最大payloadと超過拒否。
7. ✅ `security_bond`: Pairing、Bond保存/削除/再接続、暗号化Characteristic。
8. ✅ `security_passkey`: 静的passkey、MITM、認証必須Characteristic。
9. ✅ `hid_keyboard_device`: HID Keyboard Device、Battery Read、Input Notification、Output Report。
10. ✅ `hid_keyboard_host`: HID Host、全6 profileのReport Map解析、Vendor Input / Output / Feature、state、LED返送。
11. ✅ `ble_keybridge_keyboard`: ESP32KeyBridge input adapter試作、remap、切断release、LED返送、Bond再接続。
12. ✅ `lifecycle_stress`: event flood中の切断保持、再接続heap、GATT/connect実行中の`end()`、scanner flush、接続timeoutの非同期失敗、GATT操作排他、peer loss（supervision timeout）。
13. ✅ `hid_robustness`: CCCD購読gate、rollover無視、queue満杯時の全release保持、Discovery中disconnect拒否、config違いの再`begin()`拒否。
14. ✅ `hid_security`: security有効HID Deviceが未暗号化linkのRead/Discovery/Inputを拒否。
15. ✅ `hid_boot_keyboard`: Report IDなしboot keyboardのDiscoveryと入力、長さ異常reportのカウント。
16. ✅ `advertise_payload`: raw advertisementのAD構造検証（単一Complete List、type重複なし）。
17. ✅ host unit test（`tests/unit/`）: keymap変換（Unicode 4-plane、AltGr、文字ペアCapsLock）とHID Report Map parser。
18. ✅ `battery_service`: standalone Battery LevelのRead、CCCD購読、Notification、解除。
19. ✅ `device_information`: standalone DISの文字列Readと7-byte little-endian PnP ID decode。
20. ✅ `current_time`: standalone Current Timeの10-byte decode、CCCD購読、Notification、解除。
21. ✅ `heart_rate`: Body Sensor Location Readとflags付き可変長Measurementの購読、decode、解除。
22. ✅ `environmental_sensing`: Temperature / Humidity / Pressureのscale値Read、温度Notification、解除。
23. ✅ `hid_keyboard_nkro`: 29-byte bitmap Report、8キー同時押し、高usage、個別release、LED Output。
24. ✅ `midi_device`: 同梱API CentralによるEspBleMidiDeviceのwire形式（header＋timestamp＋Note On/Off）、空Read、複数パケットSysExの独立reassemble、Centralからの書込みdecode検証。
25. ✅ `midi_host`: 同梱API Peripheralからのrunning statusパケットをEspBleMidiHostが2メッセージへdecode、Host送信Note Onと複数パケットSysExのPeripheral到達検証。
26. ✅ `health_thermometer`: Health Thermometer ServerのIEEE-11073 FLOAT Temperature Measurement Indicate、ClientのType Read・Indication購読・FLOAT decode検証。
27. ✅ `blood_pressure`: Blood Pressure ServerのIEEE-11073 SFLOAT systolic/diastolic/mean Measurement Indicate、ClientのFeature Read・Indication購読・SFLOAT decode検証。
28. ✅ `weight_scale`: Weight Scale Serverの0.005 kg分解能uint16 Weight Measurement Indicate、ClientのFeature Read・Indication購読・decode検証。
29. ✅ `cycling_speed_cadence`: CSC Serverの多フィールドMeasurement Notify、ClientのSensor Location Read・Notification購読・wheel/crank全フィールドdecode検証。
30. ✅ `running_speed_cadence`: RSC Serverの混在幅Measurement Notify、ClientのSensor Location Read・Notification購読・speed/cadence/stride/distance decode検証。
31. ✅ `cycling_power`: Cycling Power Serverの16bit flags＋符号付き16bit power Measurement Notify、ClientのSensor Location Read・Notification購読・負のpower decode検証。
32. ✅ `pulse_oximeter`: Pulse Oximeter ServerのSFLOAT SpO2/pulse rate Spot-Check Indicate、ClientのPLX Features Read・Indication購読・SFLOAT decode検証。
33. ✅ `glucose`: RACP手続き。ClientのRACP write→ServerのMeasurement notify→RACP応答indicateの一連の振る舞いと、sequence/base time/SFLOAT濃度のdecode検証。
34. ✅ `body_composition`: Body Composition Serverのuint16 flags＋必須Body Fat Percentage（0.1 %/LSB）＋任意Weight（flag bit 10）Measurement Indicate、ClientのFeature Read・Indication購読・全フィールドdecode検証。
35. ✅ `location_navigation`: Location and Navigation Serverのflags駆動Location and Speed Notify（Instantaneous Speed＋sint32緯度経度）、ClientのLN Feature Read・Notification購読・speed/lat/lon decode検証。
36. ✅ `user_data`: User Data ServiceでClientがFirst NameとAgeをWrite→Serverの`onWritten`受信→Database Change Increment（uint32）をNotify→ClientがincrementをdecodeしAgeを再readして書き込み保存を確認。書き込み→onWritten→notifyパス検証。
37. ✅ `alert_notification`: Alert Notification ServiceでClientがSupported New Alert Category bitmask（0x0022）をRead・New Alert購読・Control Pointへ「Notify New Alert Immediately」をWrite→Serverがcategory/count/text（"Bob"）付きNew AlertをNotify→Clientがdecode。Control Point→notifyパス検証。
38. ✅ `immediate_alert`: Immediate Alert Service（Find Me）でClientがAlert Level（0x2A06）へHigh Alert（2）→No Alert（0）をWrite Without Responseで書き込み→Serverが`onWritten`で各levelをloop contextで受信。Write Without Responseのみの標準Service検証。

## 合格条件

- test codeがすべての入力を生成し、Serial assertionで結果を判定する。
- timeoutやretryを含む合否条件が固定されている。
- EspBle同士だけでなく、同梱BLE API直接実装との組み合わせがある。
- 手動確認が必要な項目を自動テスト合格条件へ混ぜない。
