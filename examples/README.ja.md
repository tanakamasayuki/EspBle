# EspBle Examples

> English: [README.md](README.md)

## BLEとは

Bluetooth Low Energy（BLE）は、小さなデータを低消費電力でやり取りするための無線規格です。名前は似ていますが、イヤホンやSPP（Serial Port Profile）で使われてきた**Bluetooth Classicとは別物の通信方式**で、互換性はありません。

- **Bluetooth Classic**: 常時接続のストリーム通信。音声（A2DP/HFP）やシリアル（SPP）向け。消費電力が大きい。
- **BLE**: 必要なときだけ短く通信するイベント指向。センサー値、キー入力、設定値など「小さいデータのやり取り」向け。ボタン電池で年単位の動作を狙える。

EspBleはArduino-ESP32同梱のNimBLE backendを使う**BLE専用ライブラリ**です。Bluetooth ClassicのA2DP/HFP/**SPPは使えません**。「BLEでシリアル通信っぽいことをしたい」場合は、Nordic UART Service（NUS）互換などのGATTベースの手法を使います（EspBleでは将来の拡張候補です）。

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

- **HID Keyboard Device**: ESP32をBLEキーボードにする。PC/スマホからは普通のキーボードに見える。
- **HID Keyboard Host**: ESP32が市販BLEキーボードの親機になる。キー入力をraw usage（物理キー番号）と、レイアウト変換した文字（Unicode/ASCII、19レイアウト対応）の両方で受け取れる。

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
| [Security/JustWorksServer](Security/JustWorksServer/) | Peripheral | Just Works Pairing + Bondingと暗号化Characteristic |
| [Security/StaticPasskeyServer](Security/StaticPasskeyServer/) | Peripheral | 静的passkeyによるMITM認証Characteristic（表示側） |
| [Security/StaticPasskeyClient](Security/StaticPasskeyClient/) | Central | passkey入力側。`requestSecurity()`と認証必須Read |
| [Hid/KeyboardDevice](Hid/KeyboardDevice/) | HID Device | SerialコマンドでキーをタイプするBLE keyboard。LED report受信 |
| [Hid/KeyboardHost](Hid/KeyboardHost/) | HID Host | BLE keyboardへ接続してキー表示、LED書込み |
| [Info/ScanDump](Info/ScanDump/) | 診断 | advertisementの全フィールド（UUID・Manufacturer Data等）をダンプ |
| [Info/ConnectionInspector](Info/ConnectionInspector/) | 診断 | 対話式に接続してMTU・security状態・Bond・カウンタをダンプ |

2台のボードでの推奨ペア:

- Gap/Advertise ↔ Gap/Scan
- Gatt/Server ↔ Gatt/Client
- Gatt/NotifyServer ↔ Gatt/SubscribeClient（およびGap/Mtu）
- Gatt/IndicateServer ↔ Gatt/IndicateClient
- Security/StaticPasskeyServer ↔ Security/StaticPasskeyClient
- Hid/KeyboardDevice ↔ Hid/KeyboardHost
- Info/ScanDump・Info/ConnectionInspectorは任意の相手（他のexampleやスマートフォン、市販BLE機器）の観察に使えます
