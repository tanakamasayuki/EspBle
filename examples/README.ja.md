# EspBle Examples

> English: [README.md](README.md)

## BLEとは

Bluetooth Low Energy（BLE）は、小さなデータを低消費電力でやり取りするための無線規格です。名前は似ていますが、イヤホンやSPP（Serial Port Profile）で使われてきた**Bluetooth Classicとは別物の通信方式**で、互換性はありません。

- **Bluetooth Classic**: 常時接続のストリーム通信。音声（A2DP/HFP）やシリアル（SPP）向け。消費電力が大きい。
- **BLE**: 必要なときだけ短く通信するイベント指向。センサー値、キー入力、設定値など「小さいデータのやり取り」向け。ボタン電池で年単位の動作を狙える。

EspBleはArduino-ESP32同梱のNimBLE backendを使う**BLE専用ライブラリ**です。Bluetooth ClassicのA2DP/HFP/**SPPは使えません**。「BLEでシリアル通信っぽいことをしたい」場合は、[NUS互換Server](Gatt/NusServer/) / [Client](Gatt/NusClient/)のようなGATTベースの手法を使います。

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

| Example | Role | 説明 |
|---|---|---|
| [CompileSmoke](CompileSmoke/) | — | 最小のビルド確認。ライブラリバージョンを表示 |
| [Gap/Advertise](Gap/Advertise/) | Peripheral | 名前+Service UUIDつきのconnectable Legacy Advertising |
| [Gap/Scan](Gap/Scan/) | Central | address / RSSI / nameを表示する継続active scan |
| [Gap/Connect](Gap/Connect/) | Central | Service UUIDをscanして接続。非同期の接続/切断/失敗イベント |
| [Gap/Mtu](Gap/Mtu/) | Central | 希望MTUの交換とNotification payload上限 |
| [Gatt/Server](Gatt/Server/) | Peripheral | Read/Write可能なCharacteristicを持つ独自Service |
| [Gatt/Client](Gatt/Client/) | Central | Gatt/Serverに対する既知UUID Discovery → Read → Writeの連鎖 |
| [Gatt/NotifyServer](Gatt/NotifyServer/) | Peripheral | 購読状態でgateした周期Notification |
| [Gatt/SubscribeClient](Gatt/SubscribeClient/) | Central | NotifyServerを購読してNotificationを表示 |
| [Gatt/IndicateServer](Gatt/IndicateServer/) | Peripheral | 確認応答つきのIndication配信と`onSent()`での配信確認 |
| [Gatt/IndicateClient](Gatt/IndicateClient/) | Central | IndicateServerのIndicationを購読 |
| [Gatt/BatteryServer](Gatt/BatteryServer/) | Peripheral | 標準Battery LevelのReadとNotification |
| [Gatt/BatteryClient](Gatt/BatteryClient/) | Central | Battery LevelのReadとNotification購読 |
| [Gatt/DeviceInfoServer](Gatt/DeviceInfoServer/) | Peripheral | 標準Device Information文字列とPnP ID |
| [Gatt/DeviceInfoClient](Gatt/DeviceInfoClient/) | Central | Device Informationの順次ReadとPnP ID decode |
| [Gatt/CurrentTimeServer](Gatt/CurrentTimeServer/) | Peripheral | 標準10-byte Current TimeのReadとNotification |
| [Gatt/CurrentTimeClient](Gatt/CurrentTimeClient/) | Central | Current TimeのdecodeとNotification購読 |
| [Gatt/HeartRateServer](Gatt/HeartRateServer/) | Peripheral | 標準Heart Rate MeasurementとBody Sensor Location |
| [Gatt/HeartRateClient](Gatt/HeartRateClient/) | Central | flagsに従うHeart Rate Measurementのdecodeと購読 |
| [Gatt/EnvironmentalServer](Gatt/EnvironmentalServer/) | Peripheral | 標準Temperature、Humidity、Pressure値 |
| [Gatt/EnvironmentalClient](Gatt/EnvironmentalClient/) | Central | scale付きSensor値ReadとTemperature Notification購読 |
| [Gatt/HealthThermometerServer](Gatt/HealthThermometerServer/) | Peripheral | IEEE-11073 FLOAT Temperature MeasurementのIndicateとTemperature Type |
| [Gatt/HealthThermometerClient](Gatt/HealthThermometerClient/) | Central | Temperature Type ReadとFLOAT測定値のIndication decode |
| [Gatt/BloodPressureServer](Gatt/BloodPressureServer/) | Peripheral | IEEE-11073 SFLOAT systolic/diastolic/meanのMeasurement IndicateとFeature |
| [Gatt/BloodPressureClient](Gatt/BloodPressureClient/) | Central | Feature ReadとSFLOAT測定値のIndication decode |
| [Gatt/WeightScaleServer](Gatt/WeightScaleServer/) | Peripheral | uint16 Weight Measurement（0.005 kg分解能）のIndicateとFeature |
| [Gatt/WeightScaleClient](Gatt/WeightScaleClient/) | Central | Feature ReadとWeight MeasurementのIndication decode |
| [Gatt/CyclingSpeedCadenceServer](Gatt/CyclingSpeedCadenceServer/) | Peripheral | 多フィールドwheel/crank CSC MeasurementのNotify、Feature、Sensor Location |
| [Gatt/CyclingSpeedCadenceClient](Gatt/CyclingSpeedCadenceClient/) | Central | Sensor location ReadとCSC MeasurementのNotification decode |
| [Gatt/RunningSpeedCadenceServer](Gatt/RunningSpeedCadenceServer/) | Peripheral | speed/cadence/stride/distance RSC MeasurementのNotify、Feature、Sensor Location |
| [Gatt/RunningSpeedCadenceClient](Gatt/RunningSpeedCadenceClient/) | Central | Sensor location ReadとRSC MeasurementのNotification decode |
| [Gatt/GlucoseServer](Gatt/GlucoseServer/) | Peripheral | Record Access Control Point: RACP write → Measurement notify → RACP indicate |
| [Gatt/GlucoseClient](Gatt/GlucoseClient/) | Central | RACPレコード要求とMeasurement/応答のdecode |
| [Gatt/CyclingPowerServer](Gatt/CyclingPowerServer/) | Peripheral | 符号付き16bit power Cycling Power MeasurementのNotify、Feature、Sensor Location |
| [Gatt/CyclingPowerClient](Gatt/CyclingPowerClient/) | Central | Sensor location Readと符号付きpower測定値のdecode |
| [Gatt/NusServer](Gatt/NusServer/) | Peripheral | NUS互換RX Write / TX Notification echo |
| [Gatt/NusClient](Gatt/NusClient/) | Central | NUS互換TX購読とRX Write Without Response |
| [Security/JustWorksServer](Security/JustWorksServer/) | Peripheral | Just Works Pairing + Bondingと暗号化Characteristic |
| [Security/StaticPasskeyServer](Security/StaticPasskeyServer/) | Peripheral | 静的passkeyによるMITM認証Characteristic（表示側） |
| [Security/StaticPasskeyClient](Security/StaticPasskeyClient/) | Central | passkey入力側。`requestSecurity()`と認証必須Read |
| [Hid/KeyboardDevice](Hid/KeyboardDevice/) | HID Device | SerialコマンドでキーをタイプするBLE keyboard。LED report受信 |
| [Hid/KeyboardHost](Hid/KeyboardHost/) | HID Host | 複合BLE HIDへ接続し、全対応Reportを種別別表示。keyboard LED書込み |
| [Hid/Mouse](Hid/Mouse/) | HID Device | 5ボタン相対mouse |
| [Hid/ConsumerControl](Hid/ConsumerControl/) | HID Device | 音量・再生/一時停止media key |
| [Hid/CompositeKeyboardMouse](Hid/CompositeKeyboardMouse/) | HID Device | keyboardとmouseを1つのHID Serviceへ複合 |
| [Hid/VendorDevice](Hid/VendorDevice/) | HID Device | Report ID 6のVendor Input / Output / Feature |
| [Hid/VendorHost](Hid/VendorHost/) | HID Host | Vendor Input受信とOutput / Feature書込み |
| [Midi/MidiDevice](Midi/MidiDevice/) | MIDI Device | BLE MIDI Peripheral: Note On/Off送信、受信MIDI表示 |
| [Midi/MidiHost](Midi/MidiHost/) | MIDI Host | BLE MIDI Central: Discovery/購読してMIDI表示、ノート送信 |
| [Info/ScanDump](Info/ScanDump/) | 診断 | advertisementの全フィールド（UUID・Manufacturer Data等）をダンプ |
| [Info/ConnectionInspector](Info/ConnectionInspector/) | 診断 | 対話式に接続してMTU・security状態・Bond・カウンタをダンプ |

2台のボードでの推奨ペア:

- Gap/Advertise ↔ Gap/Scan
- Gatt/Server ↔ Gatt/Client
- Gatt/NotifyServer ↔ Gatt/SubscribeClient（およびGap/Mtu）
- Gatt/IndicateServer ↔ Gatt/IndicateClient
- Gatt/BatteryServer ↔ Gatt/BatteryClient
- Gatt/DeviceInfoServer ↔ Gatt/DeviceInfoClient
- Gatt/CurrentTimeServer ↔ Gatt/CurrentTimeClient
- Gatt/HeartRateServer ↔ Gatt/HeartRateClient
- Gatt/EnvironmentalServer ↔ Gatt/EnvironmentalClient
- Gatt/HealthThermometerServer ↔ Gatt/HealthThermometerClient
- Gatt/BloodPressureServer ↔ Gatt/BloodPressureClient
- Gatt/WeightScaleServer ↔ Gatt/WeightScaleClient
- Gatt/CyclingSpeedCadenceServer ↔ Gatt/CyclingSpeedCadenceClient
- Gatt/RunningSpeedCadenceServer ↔ Gatt/RunningSpeedCadenceClient
- Gatt/GlucoseServer ↔ Gatt/GlucoseClient
- Gatt/CyclingPowerServer ↔ Gatt/CyclingPowerClient
- Gatt/NusServer ↔ Gatt/NusClient
- Security/StaticPasskeyServer ↔ Security/StaticPasskeyClient
- Hid/KeyboardDevice / Hid/CompositeKeyboardMouse ↔ Hid/KeyboardHost
- Hid/VendorDevice ↔ Hid/VendorHost
- Midi/MidiDevice ↔ Midi/MidiHost
- Info/ScanDump・Info/ConnectionInspectorは任意の相手（他のexampleやスマートフォン、市販BLE機器）の観察に使えます
