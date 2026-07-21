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
- `advertise_scan`: Peer側のEspBle Advertisingと親側のEspBle Scannerを使い、name、128-bit Service UUID、Manufacturer Data、Scan Resultの`update()` context配送を確認する。
- `lifecycle_stress`: 接続lifecycleの回帰テスト。`update()`停止中のNotification floodでもDisconnectedイベントの配送とHID Host slotの解放が失われないこと（drop数は`droppedEventCount()`で観測）、connect/HID discover/disconnectの反復で`BLEClient`とservice treeがリークしないこと、GATT read実行中の`end()`/`begin()`反復に耐えること、到達不能なpeerへの`connect()`が指定timeout付近で非同期失敗すること、実行中GATT操作と競合する2つ目の操作が同期的に拒否されること、peerのradioが無通告で消えてもsupervision timeoutでDisconnectedが配送されることを確認する。
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
- `cycling_speed_cadence`: 標準CSC Serverが多フィールドMeasurement（累積wheel/crank回転数とイベント時刻）をNotifyし、ClientがSensor Location Read・購読・全フィールドのデコードを行うことを確認する。
- `running_speed_cadence`: 標準RSC Serverが混在幅Measurement（uint16 speed、uint8 cadence、任意のuint16 stride length・uint32 total distance）をNotifyし、ClientがSensor Location Read・購読・存在する全フィールドのデコードを行うことを確認する。
- `cycling_power`: 標準Cycling Power Serverが16bit flagsと符号付き16bit instantaneous powerを持つMeasurementをNotifyし、ClientがSensor Location Read・購読・負のpowerの正確なデコードを行うことを確認する。
- `pulse_oximeter`: 標準Pulse Oximeter ServerがSpO2とpulse rateをIEEE-11073 16-bit SFLOATで持つSpot-Check MeasurementをIndicateし、ClientがPLX Features Read・98 % / 60 bpmの正確なデコードを行うことを確認する。
- `glucose`: Record Access Control Point手続きを検証する。Clientが「Report Stored Records（all）」を書き込み、ServerがGlucose Measurement（sequence、base time、SFLOAT濃度）を1件Notifyしてから`onSent`駆動でRACP応答をIndicateする。write → notify → indicateの一連の振る舞いを確認する。
