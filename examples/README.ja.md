# EspBle Examples

> English: [README.md](README.md)

## BLEとは

Bluetooth Low Energy（BLE）は、小さなデータを低消費電力でやり取りするための無線規格です。名前は似ていますが、イヤホンやSPP（Serial Port Profile）で使われてきた**Bluetooth Classicとは別物の通信方式**で、互換性はありません。

- **Bluetooth Classic**: 常時接続のストリーム通信。音声（A2DP/HFP）やシリアル（SPP）向け。消費電力が大きい。
- **BLE**: 必要なときだけ短く通信するイベント指向。センサー値、キー入力、設定値など「小さいデータのやり取り」向け。ボタン電池で年単位の動作を狙える。

EspBleはArduino-ESP32同梱のNimBLE backendを使う**BLE専用ライブラリ**です。Bluetooth ClassicのA2DP/HFP/**SPPは使えません**。「BLEでシリアル通信っぽいことをしたい」場合は、[NUS互換Server](Gatt/Basics/NusServer/) / [Client](Gatt/Basics/NusClient/)のようなGATTベースの手法を使います。

### GAP — 相手を見つける・つながる

GAP（Generic Access Profile）は「発見と接続」の層です。

- **Advertising**: Peripheral（周辺機器）が「ここにいるよ」と名前やService UUIDを電波で放送する。
- **Scanning**: Central（親機）が周囲の放送を受信して相手を探す。
- **接続**: CentralがScan結果から相手を選んで接続する。接続後はどちらの役割でも双方向にデータを送れる。

ESP32は1台で**CentralとPeripheralを同時に**こなせます（例: キーボードから入力を受けつつ、PCへキーボードとして見せる）。

### GATT — データのやり取り

GATT（Generic Attribute Profile）は接続後のデータ構造です。データは**Service**（機能のまとまり、例: Battery Service）と、その中の**Characteristic**（個々の値、例: Battery Level）として公開されます。

- **GATT Server**: 値を持っている側。Read/Writeに応え、値の変化を**Notification/Indication**で購読者へ配信できる。
- **GATT Client**: 値を利用する側。UUIDを指定してRead/Write/購読を行う。

「Peripheral=Server、Central=Client」が典型ですが、GATTの役割はlink roleとは独立した概念です。

### HID — キーボードやマウス

HID over GATT（HOGP）は、USBキーボード・マウスと同じHIDの仕組みをBLEに載せた標準プロファイルです。EspBleは次の両方を提供します。

- **HID Device**: keyboard、mouse、consumer/system control、gamepadを1つのHID Serviceへ複合できます。
- **HID Host**: 1つの`hidHost()`で全対応Reportを受信し、keyboardはUnicode/ASCIIへの19レイアウト変換にも対応します。

### MIDI — BLE MIDI楽器

BLE MIDIは、メッセージごとに13-bitミリ秒timestampを付けて単一のGATT characteristicでMIDIを流します。EspBleは`EspBleMidi.h`のpacket codecの上に両側を提供します。

- **MIDI Device**（`EspBleMidiDevice`）: BLE MIDI Peripheralをadvertiseし、Note On/Off、Control Change等を送信します。
- **MIDI Host**（`EspBleMidiHost`）: BLE MIDI Peripheralへ接続・購読し、デコード済みメッセージ（running statusやSystem Real-Timeも処理）を受信します。

APIは[EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) / [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost)のMIDIクラスに揃えており、USBとBLEでコードを移植できます。

### Security — Pairing・Bonding・暗号化

BLEのセキュリティは接続ごとに確立します。

- **Pairing**: 鍵を交換してlinkを暗号化する手続き。確認なしの**Just Works**と、6桁passkeyで相手を確認する**MITM認証**（passkey entry）がある。
- **Bonding**: 交換した鍵を保存し、次回から自動で暗号化する。
- **属性の保護**: Characteristicごとに「暗号化必須」「認証必須」を宣言でき、満たさないアクセスはエラーになる（HIDキーボードは暗号化必須が標準）。

EspBleでは`EspBleConfig::security`で有効化し、Characteristic側は`encryptedRead/Write`・`authenticatedRead/Write`で宣言します。[Security/JustWorksServer](Security/JustWorksServer/)と[Security/StaticPasskeyServer](Security/StaticPasskeyServer/)が最小構成です。

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

### GAP — advertise / scan / connect

| Example | Role | 説明 |
|---|---|---|
| [Gap/Advertise](Gap/Advertise/) | Peripheral | 名前+Service UUIDつきのconnectable Legacy Advertising |
| [Gap/Scan](Gap/Scan/) | Central | address / RSSI / nameを表示する継続active scan |
| [Gap/Connect](Gap/Connect/) | Central | Service UUIDをscanして接続。非同期の接続/切断/失敗イベント |
| [Gap/Mtu](Gap/Mtu/) | Central | 希望MTUの交換とNotification payload上限 |
| [Gap/Beacon](Gap/Beacon/) | Broadcaster | manufacturer data＋間隔制御のnon-connectable・non-scannable beacon |
| [Gap/IBeacon](Gap/IBeacon/) | Broadcaster | Apple iBeacon（UUID / major / minor / measured power）をbroadcast |
| [Gap/PrivateAddress](Gap/PrivateAddress/) | Peripheral | random static / resolvable private addressでadvertise |

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
| [Hid/CompositeKeyboardMouse](Hid/CompositeKeyboardMouse/) | HID Device | keyboardとmouseを1つのHID Serviceへ複合 |
| [Hid/VendorDevice](Hid/VendorDevice/) | HID Device | Report ID 6のVendor Input / Output / Feature |
| [Hid/VendorHost](Hid/VendorHost/) | HID Host | Vendor Input受信とOutput / Feature書込み |
| [Hid/CustomDevice](Hid/CustomDevice/) | HID Device | `ble.hidCustom()`で任意Report Descriptor（入力＋出力Report） |
| [Hid/CustomClient](Hid/CustomClient/) | GATT Client | Custom HIDのReport Mapを読み、入力Reportをデコード |

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
- Hid/KeyboardDevice / Hid/CompositeKeyboardMouse / Hid/KeyboardNkro ↔ Hid/KeyboardHost
- Hid/VendorDevice ↔ Hid/VendorHost
- Hid/CustomDevice ↔ Hid/CustomClient
- Midi/MidiDevice ↔ Midi/MidiHost
- Info/ScanDump・Info/ConnectionInspectorは任意の相手（他のexampleやスマートフォン、市販BLE機器）の観察に使えます
