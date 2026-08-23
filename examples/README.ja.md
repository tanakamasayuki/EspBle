# EspBle Examples

> English: [README.md](README.md)

## まずガイドを読んでください

exampleを動かす前に、まず[BLE通信の入門ガイド](../docs/GUIDE_BLE_BASICS.ja.md)を読んでみてください。GAP（探してつながる）、セキュリティ（ペアリングとボンディング）、GATT（データのやり取り）、UUID、HID、BLE MIDIまで、コードの背景にある仕組みと用語を本格的に説明しています。ガイドで概念をつかんでから対応するexampleのREADMEとコードを見ると、APIを呼ぶ理由や実行時に起きていることまで理解しやすくなります。

### ガイド・利用者向け資料一覧

`docs/`には入門ガイド以外にも、方式の選び方、対応機能、個別profileの仕様、対象構成ごとの制限を詳しくまとめています。

| 文書 | まず読む場面 |
|---|---|
| [BLE通信の入門ガイド](../docs/GUIDE_BLE_BASICS.ja.md) | **最初に読むガイド。** BLEの全体像からGAP・セキュリティ・GATT・UUID・HID・BLE MIDIまで |
| [BLEとBluetooth Classicのどちらを使うか](../docs/CLASSIC_VS_BLE.ja.md) | 新しく作る機器でどちらを選ぶか、HIDなど両方にある機能がどう違うかを判断するとき |
| [Bluetooth Classic通信の入門ガイド](../docs/GUIDE_CLASSIC_BASICS.ja.md) | 無印ESP32でinquiry・無線設定・SPP・セキュリティ・HID・A2DP・HFPを使うとき |
| [機能対応マトリクス](../docs/FEATURE_MATRIX.ja.md) | 実装・検証済み、exampleで実現可能、部分対応、対象外の機能を一覧で確認するとき |
| [HID Report Descriptorを書く](../docs/GUIDE_HID_DESCRIPTORS.ja.md) | 独自HIDや複合HIDを作り、Report Descriptorを設計・検証するとき |
| [HID Device仕様](../docs/HID_DEVICE_SPEC.ja.md) / [HID Host仕様](../docs/HID_HOST_SPEC.ja.md) | HIDのprofile構成、Report ID、送受信API、Discoveryとevent配送を正確に確認するとき |
| [他のライブラリからEspBleへ](../docs/GUIDE_MIGRATION.ja.md) | `BLEDevice`系・NimBLE-Arduino・`BluetoothSerial`のコードを移行するとき |
| [EspBleを深く使う](../docs/GUIDE_ADVANCED.ja.md) | 基本exampleの次に、callbackの実行context・上限・backpressure・再接続・debugを理解するとき |
| [ESP32-P4 / ESP-Hostedセットアップ](../docs/ESP_HOSTED_SETUP.ja.md) | P4＋C6構成の準備、配線、firmware更新を行うとき |
| [ESP32-P4 / ESP-Hostedの既知制限](../docs/ESP_HOSTED_LIMITATIONS.ja.md) | P4＋C6構成で実機確認済みの範囲やSecurity・MTUなどの制限を確認するとき |
| [用語と命名規則](../docs/TERMINOLOGY.ja.md) | Central / Peripheral、GATT Client / Server、HID Host / Deviceの役割を整理するとき |
| [ドキュメント全体の案内](../docs/README.ja.md) | 仕様書・機能表・設計資料を含むすべての文書から目的別に探すとき |

### BLE入門ガイドとexampleの対応

| 知りたいこと | ガイドの章 | 対応するexample |
|---|---|---|
| BLEとは何か、Classicとの違い | 1章 | — |
| Advertising・Scan・接続・アドレス | 2章 GAP編 | [Gap/](Gap/) |
| ペアリング・ボンディング・認証方式 | 3章 セキュリティ編 | [Security/](Security/) |
| Service・Characteristic・Read/Write・Notify | 4章 GATT編 | [Gatt/](Gatt/) |
| UUIDの標準形と独自形 | 5章 | — |
| キーボード・マウスとして振る舞う／入力を受け取る | 6章 HID編 | [Hid/](Hid/) |
| BLE MIDI楽器 | 7章 BLE MIDI編 | [Midi/](Midi/) |
| P4/C6 ESP-HostedのSDIO pinを指定する | ESP-Hostedセットアップ | [Hosted/CustomPins](Hosted/CustomPins/) |

おすすめの順序は、BLE入門ガイドの該当章 → 対応するexampleのREADME → `.ino`のコードです。各exampleのREADMEには必要なボード、実行方法、期待される出力もまとめています。

## ビルド方法

各exampleには検証済みArduino-ESP32バージョンを固定した`sketch.yaml` profileが同梱されているため、IDEのボード設定なしでビルドできます:

```sh
arduino-cli compile --profile esp32s3 examples/<path>
```

## 一覧

分野ごとにグループ分けしています。各標準Serviceのディレクトリには対になる`…Server`（Peripheral）と`…Client`（Central）があり、2台のボードでペアにして動かします。

### はじめに

| Example | Role | 説明 |
|---|---|---|
| [CompileSmoke](CompileSmoke/) | — | 最小のビルド確認。ライブラリバージョンを表示 |
| [Hosted/CustomPins](Hosted/CustomPins/) | P4 Host | board variantと異なるESP-Hosted SDIO pinを`ble.begin()`前に上書き |
| [Hosted/WifiCoexistence](Hosted/WifiCoexistence/) | P4 Host | 1つの共有ESP-Hosted transportでWi-FiとBLEを同時利用し、停止順序を示す |

### GAP — advertise / scan / connect

| Example | Role | 説明 |
|---|---|---|
| [Gap/Advertise](Gap/Advertise/) | Peripheral | 名前+Service UUIDつきのconnectable Legacy Advertising |
| [Gap/Scan](Gap/Scan/) | Central | address / RSSI / nameを表示する継続active scan |
| [Gap/Connect](Gap/Connect/) | Central | Service UUIDをscanして接続。非同期の接続/切断/失敗イベント |
| [Gap/Mtu](Gap/Mtu/) | Central | 希望MTUの交換とNotification payload上限 |
| [Gap/ConnectionParameters](Gap/ConnectionParameters/) | Central | 接続後のinterval / latency / timeoutとPHYの変更 |
| [Gap/Beacon](Gap/Beacon/) | Broadcaster | manufacturer data＋間隔制御のnon-connectable・non-scannable beacon |
| [Gap/IBeacon](Gap/IBeacon/) | Broadcaster | Apple iBeacon（UUID / major / minor / measured power）をbroadcast |
| [Gap/ServiceData](Gap/ServiceData/) | Broadcaster | Service Data（AD 0x16）で温度を放送。接続させずに値を配る |
| [Gap/ScanResponse](Gap/ScanResponse/) | Peripheral | advertising payloadとscan responseの2面に分けて31byte制限を回避 |
| [Gap/AcceptList](Gap/AcceptList/) | Peripheral | Filter Accept Listで接続できる相手を制限 |
| [Gap/DirectedAdvertise](Gap/DirectedAdvertise/) | Peripheral | 相手を1台に指定したDirected Advertising。payloadは載らない |
| [Gap/PrivateAddress](Gap/PrivateAddress/) | Peripheral | random static / resolvable private addressでadvertise |
| [Gap/MultiConnection](Gap/MultiConnection/) | Central | 複数のperipheral接続を同時に保持し、接続ごとのIDで指定する |

### GATT — 基本（汎用の仕組み＋シリアル）

| Example | Role | 説明 |
|---|---|---|
| [Gatt/Basics/Server](Gatt/Basics/Server/) | Peripheral | Read/Write可能なCharacteristicを持つ独自Service |
| [Gatt/Basics/Client](Gatt/Basics/Client/) | Central | Serverに対する既知UUID Discovery → Read → Writeの連鎖 |
| [Gatt/Basics/NotifyServer](Gatt/Basics/NotifyServer/) | Peripheral | 購読状態でgateした周期Notification |
| [Gatt/Basics/SubscribeClient](Gatt/Basics/SubscribeClient/) | Central | NotifyServerを購読してNotificationを表示 |
| [Gatt/Basics/AutoReconnectClient](Gatt/Basics/AutoReconnectClient/) | Central | auto-reconnect + persistent subscription: 切断後にNotificationが自動再開 |
| [Gatt/Basics/IndicateServer](Gatt/Basics/IndicateServer/) | Peripheral | 確認応答つきのIndication配信と`onSent()`での配信確認 |
| [Gatt/Basics/IndicateClient](Gatt/Basics/IndicateClient/) | Central | IndicateServerのIndicationを購読 |
| [Gatt/Basics/NusServer](Gatt/Basics/NusServer/) | Peripheral | NUS互換RX Write / TX Notification echo |
| [Gatt/Basics/NusClient](Gatt/Basics/NusClient/) | Central | NUS互換TX購読とRX Write Without Response |

### GATT — デバイス情報・時刻・管理

| Example | Role | 説明 |
|---|---|---|
| [Gatt/Device/BatteryServer](Gatt/Device/BatteryServer/) | Peripheral | 標準Battery LevelのReadとNotification |
| [Gatt/Device/BatteryClient](Gatt/Device/BatteryClient/) | Central | Battery LevelのReadとNotification購読 |
| [Gatt/Device/DeviceInfoServer](Gatt/Device/DeviceInfoServer/) | Peripheral | 標準Device Information文字列とPnP ID |
| [Gatt/Device/DeviceInfoClient](Gatt/Device/DeviceInfoClient/) | Central | Device Informationの順次ReadとPnP ID decode |
| [Gatt/Device/UserDataServer](Gatt/Device/UserDataServer/) | Peripheral | Age・First Nameのread/write、書き込み時にDatabase Change IncrementをNotify |
| [Gatt/Device/UserDataClient](Gatt/Device/UserDataClient/) | Central | Age/First NameのWriteとDatabase Change IncrementのNotification観測 |
| [Gatt/Device/BondManagementServer](Gatt/Device/BondManagementServer/) | Peripheral | Bond Management Feature Read、Control Pointのbond削除op code |
| [Gatt/Device/BondManagementClient](Gatt/Device/BondManagementClient/) | Central | Feature bit fieldのReadとbond削除op codeのWrite |
| [Gatt/Time/CurrentTimeServer](Gatt/Time/CurrentTimeServer/) | Peripheral | 標準10-byte Current TimeのReadとNotification |
| [Gatt/Time/CurrentTimeClient](Gatt/Time/CurrentTimeClient/) | Central | Current TimeのdecodeとNotification購読 |
| [Gatt/Time/ReferenceTimeUpdateServer](Gatt/Time/ReferenceTimeUpdateServer/) | Peripheral | Time Update Control Pointがread可能なTime Update Stateを駆動 |
| [Gatt/Time/ReferenceTimeUpdateClient](Gatt/Time/ReferenceTimeUpdateClient/) | Central | reference update要求/キャンセルとTime Update StateのRead |

### GATT — センサー

| Example | Role | 説明 |
|---|---|---|
| [Gatt/Sensors/EnvironmentalServer](Gatt/Sensors/EnvironmentalServer/) | Peripheral | 標準Temperature、Humidity、Pressure値 |
| [Gatt/Sensors/EnvironmentalClient](Gatt/Sensors/EnvironmentalClient/) | Central | scale付きSensor値ReadとTemperature Notification購読 |

### GATT — ヘルスケア・体組成

| Example | Role | 説明 |
|---|---|---|
| [Gatt/Health/HeartRateServer](Gatt/Health/HeartRateServer/) | Peripheral | 標準Heart Rate MeasurementとBody Sensor Location |
| [Gatt/Health/HeartRateClient](Gatt/Health/HeartRateClient/) | Central | flagsに従うHeart Rate Measurementのdecodeと購読 |
| [Gatt/Health/HealthThermometerServer](Gatt/Health/HealthThermometerServer/) | Peripheral | IEEE-11073 FLOAT Temperature MeasurementのIndicateとTemperature Type |
| [Gatt/Health/HealthThermometerClient](Gatt/Health/HealthThermometerClient/) | Central | Temperature Type ReadとFLOAT測定値のIndication decode |
| [Gatt/Health/BloodPressureServer](Gatt/Health/BloodPressureServer/) | Peripheral | IEEE-11073 SFLOAT systolic/diastolic/meanのMeasurement IndicateとFeature |
| [Gatt/Health/BloodPressureClient](Gatt/Health/BloodPressureClient/) | Central | Feature ReadとSFLOAT測定値のIndication decode |
| [Gatt/Health/WeightScaleServer](Gatt/Health/WeightScaleServer/) | Peripheral | uint16 Weight Measurement（0.005 kg分解能）のIndicateとFeature |
| [Gatt/Health/WeightScaleClient](Gatt/Health/WeightScaleClient/) | Central | Feature ReadとWeight MeasurementのIndication decode |
| [Gatt/Health/BodyCompositionServer](Gatt/Health/BodyCompositionServer/) | Peripheral | Body Fat Percentage＋任意Weight MeasurementのIndicateとFeature |
| [Gatt/Health/BodyCompositionClient](Gatt/Health/BodyCompositionClient/) | Central | Feature ReadとBody Fat Percentage / Weight測定値のdecode |
| [Gatt/Health/PulseOximeterServer](Gatt/Health/PulseOximeterServer/) | Peripheral | SFLOAT SpO2/pulse-rate Spot-Check MeasurementのIndicateとFeatures |
| [Gatt/Health/PulseOximeterClient](Gatt/Health/PulseOximeterClient/) | Central | Features ReadとSpO2/pulse-rate測定値のdecode |
| [Gatt/Health/GlucoseServer](Gatt/Health/GlucoseServer/) | Peripheral | Record Access Control Point: RACP write → Measurement notify → RACP indicate |
| [Gatt/Health/GlucoseClient](Gatt/Health/GlucoseClient/) | Central | RACPレコード要求とMeasurement/応答のdecode |
| [Gatt/Health/ContinuousGlucoseMonitoringServer](Gatt/Health/ContinuousGlucoseMonitoringServer/) | Peripheral | E2E-CRC保護のCGM FeatureとCGM MeasurementのNotify |
| [Gatt/Health/ContinuousGlucoseMonitoringClient](Gatt/Health/ContinuousGlucoseMonitoringClient/) | Central | E2E-CRC検証とSFLOAT血糖値/time offsetのdecode |

### GATT — フィットネス・自転車

| Example | Role | 説明 |
|---|---|---|
| [Gatt/Fitness/CyclingSpeedCadenceServer](Gatt/Fitness/CyclingSpeedCadenceServer/) | Peripheral | 多フィールドwheel/crank CSC MeasurementのNotify、Feature、Sensor Location |
| [Gatt/Fitness/CyclingSpeedCadenceClient](Gatt/Fitness/CyclingSpeedCadenceClient/) | Central | Sensor Location ReadとCSC MeasurementのNotification decode |
| [Gatt/Fitness/RunningSpeedCadenceServer](Gatt/Fitness/RunningSpeedCadenceServer/) | Peripheral | speed/cadence/stride/distance RSC MeasurementのNotify、Feature、Sensor Location |
| [Gatt/Fitness/RunningSpeedCadenceClient](Gatt/Fitness/RunningSpeedCadenceClient/) | Central | Sensor Location ReadとRSC MeasurementのNotification decode |
| [Gatt/Fitness/CyclingPowerServer](Gatt/Fitness/CyclingPowerServer/) | Peripheral | 符号付き16bit power Cycling Power MeasurementのNotify、Feature、Sensor Location |
| [Gatt/Fitness/CyclingPowerClient](Gatt/Fitness/CyclingPowerClient/) | Central | Sensor Location Readと符号付きpower測定値のdecode |
| [Gatt/Fitness/FitnessMachineServer](Gatt/Fitness/FitnessMachineServer/) | Peripheral | Fitness Machine（FTMS）Indoor Bike DataのNotifyとFeature |
| [Gatt/Fitness/FitnessMachineClient](Gatt/Fitness/FitnessMachineClient/) | Central | Feature Readとflags駆動Indoor Bike Data（speed/cadence/power）のdecode |
| [Gatt/Fitness/LocationNavigationServer](Gatt/Fitness/LocationNavigationServer/) | Peripheral | Location and SpeedのNotify（速度＋sint32緯度経度）とLN Feature |
| [Gatt/Fitness/LocationNavigationClient](Gatt/Fitness/LocationNavigationClient/) | Central | LN Feature ReadとLocation and SpeedのNotification decode |

### GATT — アラート・近接

| Example | Role | 説明 |
|---|---|---|
| [Gatt/Alerts/AlertNotificationServer](Gatt/Alerts/AlertNotificationServer/) | Peripheral | category bitmask Read、Control Point write、New AlertのNotify |
| [Gatt/Alerts/AlertNotificationClient](Gatt/Alerts/AlertNotificationClient/) | Central | Control Point「Notify New Alert Immediately」とNew Alertのdecode |
| [Gatt/Alerts/ImmediateAlertServer](Gatt/Alerts/ImmediateAlertServer/) | Peripheral | Find Meターゲット: Alert LevelのWrite Without Response処理 |
| [Gatt/Alerts/ImmediateAlertClient](Gatt/Alerts/ImmediateAlertClient/) | Central | Find Me locator: Write Without ResponseでAlert Levelを鳴動/解除 |
| [Gatt/Alerts/PhoneAlertStatusServer](Gatt/Alerts/PhoneAlertStatusServer/) | Peripheral | Alert Status / Ringer SettingのNotify、Ringer Control PointでSilent Mode |
| [Gatt/Alerts/PhoneAlertStatusClient](Gatt/Alerts/PhoneAlertStatusClient/) | Central | Alert Status Read、Ringer Control Point操作、Ringer Settingのdecode |
| [Gatt/Alerts/ProximityServer](Gatt/Alerts/ProximityServer/) | Peripheral | Proximity Reporter: Link Loss Alert Level＋Tx Power（2 Service） |
| [Gatt/Alerts/ProximityClient](Gatt/Alerts/ProximityClient/) | Central | Proximity Monitor: Tx Power ReadとLink Loss Alert Levelのarm |

### HID over GATT

| Example | Role | 説明 |
|---|---|---|
| [Hid/KeyboardDevice](Hid/KeyboardDevice/) | HID Device | SerialコマンドでキーをタイプするBLE keyboard。LED report受信 |
| [Hid/KeyboardHost](Hid/KeyboardHost/) | HID Host | 複合BLE HIDへ接続し、全対応Reportを種別別表示。keyboard LED書込み |
| [Hid/KeyboardNkro](Hid/KeyboardNkro/) | HID Device | N-key rollover keyboard（29-byte bitmap report） |
| [Hid/Mouse](Hid/Mouse/) | HID Device | 5ボタン相対mouse |
| [Hid/ConsumerControl](Hid/ConsumerControl/) | HID Device | 音量・再生/一時停止media key |
| [Hid/Gamepad](Hid/Gamepad/) | HID Device | 6軸・hat switch・32 button |
| [Hid/CompositeKeyboardMouse](Hid/CompositeKeyboardMouse/) | HID Device | keyboardとmouseを1つのHID Serviceへ複合 |
| [Hid/VendorDevice](Hid/VendorDevice/) | HID Device | Report ID 6のVendor Input / Output / Feature |
| [Hid/VendorHost](Hid/VendorHost/) | HID Host | Vendor Input受信とOutput / Feature書込み |
| [Hid/CustomDevice](Hid/CustomDevice/) | HID Device | `ble.hidCustom()`で任意Report Descriptor（入力＋出力Report） |
| [Hid/CustomClient](Hid/CustomClient/) | GATT Client | Custom HIDのReport Mapを読み、Report Referenceをhandle指定で読んで役割を判定し、入力Reportをデコード |

### MIDI

| Example | Role | 説明 |
|---|---|---|
| [Midi/MidiDevice](Midi/MidiDevice/) | MIDI Device | BLE MIDI Peripheral: Note On/Off送信、受信MIDI表示 |
| [Midi/MidiHost](Midi/MidiHost/) | MIDI Host | BLE MIDI Central: Discovery/購読してMIDI表示、ノート送信 |

### Security

| Example | Role | 説明 |
|---|---|---|
| [Security/JustWorksServer](Security/JustWorksServer/) | Peripheral | Just Works Pairing + Bondingと暗号化Characteristic |
| [Security/StaticPasskeyServer](Security/StaticPasskeyServer/) | Peripheral | 静的passkeyによるMITM認証Characteristic（表示側） |
| [Security/StaticPasskeyClient](Security/StaticPasskeyClient/) | Central | passkey入力側。`requestSecurity()`と認証必須Read |
| [Security/RuntimePasskeyServer](Security/RuntimePasskeyServer/) | Peripheral | Pairingごとに生成されるpasskeyの表示側 |
| [Security/RuntimePasskeyClient](Security/RuntimePasskeyClient/) | Central | `providePasskey()`で実行時にpasskeyを入力する側 |
| [Security/NumericComparisonServer](Security/NumericComparisonServer/) | Peripheral | 両側に出た6桁の一致を確認するPairing（Peripheral側） |
| [Security/NumericComparisonClient](Security/NumericComparisonClient/) | Central | 同上のCentral側 |

### 診断

| Example | Role | 説明 |
|---|---|---|
| [Info/ScanDump](Info/ScanDump/) | 診断 | advertisementの全フィールド（UUID・Manufacturer Data等）をダンプ |
| [Info/ConnectionInspector](Info/ConnectionInspector/) | 診断 | 対話式に接続してMTU・security状態・Bond・カウンタをダンプ |

## 2台のボードでの推奨ペア

- Gap/Advertise ↔ Gap/Scan
- Gatt/Basics/Server ↔ Gatt/Basics/Client
- Gatt/Basics/NotifyServer ↔ Gatt/Basics/SubscribeClient / Gatt/Basics/AutoReconnectClient（およびGap/Mtu）
- Gatt/Basics/IndicateServer ↔ Gatt/Basics/IndicateClient
- Gatt/Basics/NusServer ↔ Gatt/Basics/NusClient
- 各`Gatt/<カテゴリ>/<名前>Server` ↔ 対応する`…Client`（Device、Time、Sensors、Health、Fitness、Alerts）
- Security/StaticPasskeyServer ↔ Security/StaticPasskeyClient
- Security/RuntimePasskeyServer ↔ Security/RuntimePasskeyClient
- Security/NumericComparisonServer ↔ Security/NumericComparisonClient
- Hid/KeyboardDevice / Hid/CompositeKeyboardMouse / Hid/KeyboardNkro ↔ Hid/KeyboardHost
- Hid/VendorDevice ↔ Hid/VendorHost
- Hid/CustomDevice ↔ Hid/CustomClient
- Midi/MidiDevice ↔ Midi/MidiHost
- Info/ScanDump・Info/ConnectionInspectorは任意の相手（他のexampleやスマートフォン、市販BLE機器）の観察に使えます

## 無印ESP32ならBluetooth Classicも使えます

ここまでのexampleはBLE向けです。無印ESP32では、それに加えてBluetooth Classicも使えます。
Classicのinquiry・SPP・HID・音声の仕組みは
[Classic通信の入門ガイド](../docs/GUIDE_CLASSIC_BASICS.ja.md)で説明しています。

`EspBleClassic`を使うと独自buildしたClassic hostが自動選択されます。build flagはありません。
precompiled hostの対応Coreは実測でArduino-ESP32 3.2.0〜3.3.11です（HFP audioのみ3.3.8以上）。
`EspBle`と`EspBleClassic`の両方を開始すればdual-host、片方だけなら単一hostになります。

**2つの無線は届く相手が違い、それが選択の基準になります。**BLE HID（HOGP）は2015年前後以降の
携帯・タブレット・PCが受け付けます。ClassicはBLEでは届かない相手に届きます——旧世代のゲーム機や
古いPC、car audio、headset。さらにserial port（SPP）を提供する手段と、音声を運ぶ手段
（A2DP、HFP）はClassicだけにあります。HID exampleは無線ごとに対になっており、呼び出しは
両方で同じです。手元の相手に合う方を選んでください。どちらを使うか、両方にある機能の差は
[BLEとClassic](../docs/CLASSIC_VS_BLE.ja.md)にあります。

| Example | Role | 説明 |
|---|---|---|
| [Classic/Inquiry](Classic/Inquiry/) | GAP | device discovery。addressの入手経路 |
| [Classic/RadioSettings](Classic/RadioSettings/) | GAP | 送信電力・page timeout・暗号鍵の最小長 |
| [Classic/SppServer](Classic/SppServer/) | SPP Server | binary-safeなSPP echo server |
| [Classic/SppClient](Classic/SppClient/) | SPP Client | address指定で接続し、RFCOMM channelを解決または指定する |
| [Classic/SppStream](Classic/SppStream/) | SPP Server | SPPをArduinoの`Stream`として扱う。`Serial`向けのcodeがそのまま動く |
| [Classic/SppPairing](Classic/SppPairing/) | SPP Server / GAP | applicationが制御するpairingとbond管理 |
| [Classic/HidKeyboard](Classic/HidKeyboard/) | HID Device | BLE exampleと同じprofile APIによるkeyboard / mouse |
| [Classic/HidMouse](Classic/HidMouse/) | HID Device | 移動・クリック・wheel・ドラッグ |
| [Classic/HidGamepad](Classic/HidGamepad/) | HID Device | 軸・hat・button。BLEでは代替できない用途 |
| [Classic/HidConsumerControl](Classic/HidConsumerControl/) | HID Device | メディアキーとシステム要求 |
| [Classic/HidKeyboardNkro](Classic/HidKeyboardNkro/) | HID Device | N-key rollover。6キー制限が無い |
| [Classic/HidComposite](Classic/HidComposite/) | HID Device | keyboard・mouse・メディアキーを1台で兼ねる。合成数のSDP record上限も示す |
| [Classic/HidKeyboardHost](Classic/HidKeyboardHost/) | HID Host | 相手のReport Descriptorから復号したkeyboard / mouse event |
| [Classic/HidVendorDevice](Classic/HidVendorDevice/) | HID Device | 任意Report DescriptorのClassic HID Device |
| [Classic/HidVendorHost](Classic/HidVendorHost/) | HID Host | アドレス指定で接続しraw Input Reportを受信 |
| [Classic/A2dpSinkRaw](Classic/A2dpSinkRaw/) | A2DP Sink | codec設定とencode済みSBC mediaをcallbackで受信 |
| [Classic/A2dpSource](Classic/A2dpSource/) | A2DP Source | encode済みSBC frameをbackpressure付きで送信 |
| [Classic/A2dpSinkAvrcp](Classic/A2dpSinkAvrcp/) | A2DP Sink / AVRCP TG | A2DP接続と再生操作・absolute volume |
| [Classic/AvrcpController](Classic/AvrcpController/) | AVRCP CT | 相手の再生操作、status・metadata要求 |
| [Classic/HfpClientRaw](Classic/HfpClientRaw/) | HFP Client | 単一call controlとraw CVSD/mSBC SCO transport |
| [Classic/HfpAudioGatewayRaw](Classic/HfpAudioGatewayRaw/) | HFP Audio Gateway | 小さいtelephony modelとraw CVSD/mSBC SCO transport |
