# Peer Tests

> English: [README.md](README.md)

ESP32-S3 2台をBLEで接続する自動テストです。ボード間の信号配線は不要です。

```sh
uv run --env-file .env pytest peer/
```

profileと環境変数はEspUsbHost/EspUsbDeviceの既存環境を再利用します。

| pytest上の位置 | profile | 環境変数 |
|---|---|---|
| 親側 | `s3_peer_host` | `TEST_SERIAL_PORT_S3_PEER_HOST` |
| 2台目Peer | `s3_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE` |

profile名はBLE roleを表しません。両側へsketchを転送・実行でき、両側のSerialをpytestから観測できます。初期テストでは親側sketchをCentral、`peer_device/`側sketchをPeripheralに固定し、役割交換は行いません。

## 追加済み

- `stack_smoke`: Arduino-ESP32同梱NimBLE backendのBLE APIだけを使い、Advertising/Scan、接続、GATT read/write、双方のSerialと2台fixtureを確認する。
- `beacon`: Peer側のnon-connectable・non-scannable beacon（`setConnectable(false)`＋`setScanResponseEnabled(false)`＋`setInterval(100, 150)`）がmarker Service UUIDとmanufacturer dataをbroadcastし、親側のScannerが捕捉してconnectable/scannableいずれもfalse・期待するmanufacturer payload（company `0xFFFF`＋`0x01020304`）を持つことを確認する。
- `advertise_scan`: Peer側のEspBle Advertisingと親側のEspBle Scannerを使い、name、128-bit Service UUID、Manufacturer Data、Scan Resultの`update()` context配送を確認する。
- `lifecycle_stress`: 接続lifecycleの回帰テスト。`update()`停止中のNotification floodでもDisconnectedイベントの配送とHID Host slotの解放が失われないこと（drop数は`droppedEventCount()`で観測）、connect/HID discover/disconnectの反復で`BLEClient`とservice treeがリークしないこと、GATT read実行中の`end()`/`begin()`反復に耐えること、到達不能なpeerへの`connect()`が指定timeout付近で非同期失敗すること、実行中GATT操作と競合する2つ目の操作もキューへ積まれ両方完了すること、peerのradioが無通告で消えてもsupervision timeoutでDisconnectedが配送されることを確認する。
- `hid_robustness`: HID Host/Deviceの堅牢性回帰テスト。CCCD未購読の相手へInput Reportを送らないこと、rollover(phantom) reportを全releaseと誤解釈しないこと、eventキュー満杯でも切断時の全releaseイベントが保持されること、HID Discovery中の同一接続`disconnect()`が拒否されること、異なるconfigでの2回目`begin()`が失敗することを確認する。
- `hid_security`: security有効なHID Keyboard Deviceが、未暗号化linkに対してReport Map読出し・HID Discovery・Input Report送信をすべて拒否することを確認する。
- `hid_boot_keyboard`: 汎用GATT ServerでReport IDなしのboot keyboard(Report Reference descriptorなし)を模擬し、Discoveryと入力が動作すること、長さ8以外のInput Reportが状態を変えずに`invalidInputReportCount()`へ計上されることを確認する。
- `advertise_payload`: 親側で同梱APIのpassive scanを使いraw advertisementペイロードを取得し、複数の16-bit Service UUIDが単一の「Complete List」AD構造にまとまり、同一AD typeが重複しないことを確認する。
- `battery_service`: standalone Battery Serviceの1-byte Read、CCCD購読、Notification、送信完了、解除を確認する。
- `device_information`: standalone Device Informationの文字列Readと標準7-byte little-endian PnP ID wire形式を確認する。
- `current_time`: standalone Current Timeの10-byte decode、CCCD購読、Notification、送信完了、解除を確認する。
- `heart_rate`: Body Sensor Location Readとflagsに従う16-bit心拍数、Energy Expended、RR-IntervalのNotification decodeを確認する。
- `environmental_sensing`: signed Temperature、scale付きHumidity、32-bit PressureのReadとTemperature Notification、解除を確認する。
- `midi_device`: 同梱NimBLE直接実装のCentralが`EspBleMidiDevice`のI/O characteristicを購読し、BLE MIDIのwireバイト（header＋timestamp＋Note On/Off）と空Read、独立実装のreassemblerで復元する複数パケットSysEx、Centralが書いたControl ChangeをDeviceがデコードすることを確認する。
- `midi_host`: 同梱NimBLE直接実装のPeripheralがrunning statusのBLE MIDIパケットをNotifyし、`EspBleMidiHost`がtimestamp込みで2メッセージへデコードすること、Hostの送信Note Onと複数パケットSysExがPeripheralへ届くことを確認する。
- `health_thermometer`: 標準Health Thermometer ServerがIEEE-11073 32-bit FLOATのTemperature MeasurementをIndicateし、ClientがTemperature Type Read・Indication購読・37.5℃の正確なデコードを行うことを確認する。
- `blood_pressure`: 標準Blood Pressure Serverがsystolic/diastolic/meanをIEEE-11073 16-bit SFLOATで持つMeasurementをIndicateし、ClientがFeature Read・120/80/93 mmHgの正確なデコードを行うことを確認する。
- `weight_scale`: 標準Weight Scale Serverが0.005 kg分解能のuint16 weightを持つMeasurementをIndicateし、ClientがFeature Read・70.000 kgの正確なデコードを行うことを確認する。
- `body_composition`: 標準Body Composition Serverがuint16 flags、必須のBody Fat Percentage（0.1 %/LSB）、任意のWeightフィールド（bit 10）を持つMeasurementをIndicateし、ClientがFeature Read・体脂肪率27.5 %と70.000 kgの正確なデコードを行うことを確認する。
- `location_navigation`: 標準Location and Navigation Serverがuint16 flags、Instantaneous Speed（1/100 m/s）、sint32の緯度経度（1e-7 度）を持つLocation and SpeedをNotifyし、ClientがLN Feature Read・5.00 m/sと東京座標の正確なデコードを行うことを確認する。
- `user_data`: 標準User Data Serviceがread/writeのAge、書き込み可能なFirst Name、read/write/notifyのDatabase Change Incrementを公開する。ClientがFirst NameとAgeをWriteし、Serverが各書き込みを`onWritten`で受信してincrementを増やしNotifyし、Clientがincrementが2まで増えることとAgeが書き込んだ42として再readできることを確認する。書き込み→onWritten→notifyパスを検証する。
- `alert_notification`: 標準Alert Notification Serverがreadable Supported New Alert Category bitmask（Email＋SMS/MMS、0x0022）、notifiable New Alert、writable Alert Notification Control Pointを公開する。ClientがbitmaskをRead・New Alert購読・Emailへ「Notify New Alert Immediately」をWriteし、Serverがcategory/count/text（"Bob"）付きNew AlertをNotifyしてClientがdecodeすることを確認する。Control Point→notifyパスを検証する。
- `immediate_alert`: 標準Immediate Alert Service（Find Meターゲット役）がAlert Level（0x2A06）をWrite Without Responseのuint8として公開する。ClientがHigh Alert（2）→No Alert（0）を応答なしで書き込み、Serverが各書き込みを`onWritten`でloop contextから観測することを確認する。Write Without Responseのみの標準Serviceパスを検証する。
- `phone_alert_status`: 標準Phone Alert Status Serviceがread/notifyのAlert StatusとRinger Setting、Write Without ResponseのRinger Control Pointを公開する。ClientがAlert Status（0x01）をRead・Ringer Setting購読・初期Normal値をread・Control PointでSet Silent Mode（Ringer Settingが0をNotify）とCancel Silent Mode（1をNotify）を駆動することを確認する。Control Point→状態変更notifyパスを検証する。
- `proximity`: Proximityプロファイル。1つのserverにLink Loss Service（0x1803、read/write Alert Level）とTx Power Service（0x1804、read専用signed int8 Tx Power Level）を同居させる。ClientがTx Power Level（-8 dBm）をRead・初期Alert Level（0）をread・High Alert（2）を応答ありWrite・2として再readすることを確認する。1 serverでの2 Service同居とsigned int8 readを検証する。
- `reference_time_update`: 標準Reference Time Update ServiceがWrite Without ResponseのTime Update Control Pointとread可能な2バイトのTime Update Stateを公開する。Clientが初期Idle状態（0, 0）をread・Get（Control Point 1→state 1, 0）→Cancel（Control Point 2→state 0, 1）と駆動し毎回stateを再readすることを確認する。Control Point→read専用state遷移パスを検証する。
- `bond_management`: 標準Bond Management Serviceがread可能なBond Management Feature bit field（uint24、0x000011）とwritableなBond Management Control Pointを公開する。ClientがfeatureをReadしop code 0x03（Delete bond of requesting device, LE）を応答ありWrite、Serverが`onWritten`でop codeを観測することを確認する。Feature read＋Control Point op codeのchoreographyを検証する（実bond削除ではない）。
- `continuous_glucose_monitoring`: 標準CGM ServiceがE2E-CRC保護のCGM Featureと、SFLOAT血糖値・time offset・末尾E2E-CRC（CRC-16/MCRF4XX）付きのCGM MeasurementをNotifyする。ClientはFeatureのCRCを検証してからfeature bitとtype/sample locationをdecode、Measurementを購読しそのCRCを検証して血糖値（100）とtime offset（5）をdecodeする。両基板は共有`EspBleCgmCrc.h` codecを使う。
- `disconnect_reason`: `EspBleConnection::disconnectReason`を検証する。Peripheralが切断を開始するため、開始側（Peripheral）はlocal終了、remote側（Central）はremote終了の理由コードをonDisconnectedで報告し、両者が非0かつ相異なることを確認する。グローバルGAPイベントリスナー経由で切断理由がServer/Client両パスで捕捉されることを証明する。
- `connection_parameters`: `EspBleConnection`に公開される接続パラメータ（interval/latency/timeout）と、`updateConnectionParameters()` / `onConnectionParametersUpdated()`の組を検証する。Centralが初期intervalを報告し、interval 80への更新を要求、CentralとPeripheralの双方が交渉後のinterval 80を報告することを確認する。
- `phy_update`: `EspBleConnection`に公開されるLE PHY（txPhy/rxPhy）と、`updatePhy()` / `onPhyUpdated()`の組を検証する。Centralが初期PHYを報告し、2M PHYを要求、CentralとPeripheralの双方が交渉後の2M PHY（tx/rx = 2）を報告することを確認する。
- `service_changed`: ClientがGeneric AttributeのService Changed characteristic（0x1801/0x2A05）を購読し、Serverが`notifyServicesChanged(0x0001, 0xFFFF)`を呼ぶとClientがindicationを受信して変更handle range（1..65535）をdecodeすることを確認する。Client側のService Changed処理とServer側トリガを検証する。
- `runtime_passkey`: 対話型の実行時Passkey Entry。Peripheral（DisplayOnly・MITM・静的passkeyなし）がpairingごとに動的passkeyを生成し`onPasskeyDisplayed`で提示、Central（KeyboardOnly・MITM・静的passkeyなし）はbackendのpasskey要求でブロックし、テストが表示passkeyを`providePasskey()`で中継すると両側authenticated+bondedでpairing完了することを確認する。
- `numeric_comparison`: LE Secure Connections Numeric Comparison。両側（DisplayYesNo・MITM）が同一の6桁比較値を`onNumericComparison`で提示し、テストが値の一致を確認して両側`confirmNumericComparison(true)`で確認するとauthenticated+bondedでpairing完了することを確認する。
- `hid_boot_protocol`: HID over GATTのBoot Protocolをkeyboard peripheralで検証する。generic GATT clientがProtocol Mode characteristic（0x2A4E、初期値Report Protocol Mode）をReadし、Boot Keyboard Input Report（0x2A22）を購読し、deviceをBoot Protocol Modeへ切り替え（deviceは`onProtocolMode()`で観測）、Shift+'a'の8-byte Boot ReportをNotifyで受信し、Boot Keyboard Output Report（0x2A32）のCaps Lock LEDを書き込んでdeviceが`onOutputReport()`で受け取ることを確認する。
- `hid_custom`: `ble.hidCustom()` で任意のvendor定義Report Descriptor（Report ID 1に2byte入力＋1byte出力をHID Serviceへ合成。2つのReport characteristicが同一UUID 0x2A4Dを共有）から構成したCustom HIDデバイスを検証する。generic GATT clientがserviceをdiscoverして各Reportを個別のattribute handleへ解決し、Report Map（0x2A4B）の長さを確認、入力Reportを**handleで**購読して2byteのカスタムReport（ダイヤル差分＋ボタン）をデコード、出力Reportを**handleで**書き込んでdeviceが`onOutputReport()`で受け取ることを確認する。任意の複数Report記述子と、同一UUIDを撃ち分けるhandle指定操作の両方を検証する。
- `cycling_speed_cadence`: 標準CSC Serverが多フィールドMeasurement（累積wheel/crank回転数とイベント時刻）をNotifyし、ClientがSensor Location Read・購読・全フィールドのデコードを行うことを確認する。
- `running_speed_cadence`: 標準RSC Serverが混在幅Measurement（uint16 speed、uint8 cadence、任意のuint16 stride length・uint32 total distance）をNotifyし、ClientがSensor Location Read・購読・存在する全フィールドのデコードを行うことを確認する。
- `cycling_power`: 標準Cycling Power Serverが16bit flagsと符号付き16bit instantaneous powerを持つMeasurementをNotifyし、ClientがSensor Location Read・購読・負のpowerの正確なデコードを行うことを確認する。
- `pulse_oximeter`: 標準Pulse Oximeter ServerがSpO2とpulse rateをIEEE-11073 16-bit SFLOATで持つSpot-Check MeasurementをIndicateし、ClientがPLX Features Read・98 % / 60 bpmの正確なデコードを行うことを確認する。
- `glucose`: Record Access Control Point手続きを検証する。Clientが「Report Stored Records（all）」を書き込み、ServerがGlucose Measurement（sequence、base time、SFLOAT濃度）を1件Notifyしてから`onSent`駆動でRACP応答をIndicateする。write → notify → indicateの一連の振る舞いを確認する。
