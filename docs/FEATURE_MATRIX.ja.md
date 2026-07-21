# 機能対応マトリクス

EspUsbHost / EspUsbDeviceで扱っている機能のBLE版、およびBLEで一般的に使う機能について、EspBleの対応状況を整理した一覧です。優先順位の確定版は[REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)と[DECISIONS.ja.md](DECISIONS.ja.md)を正とし、この表は俯瞰用のたたき台です。

## 凡例

| 記号 | 意味 |
|---|---|
| ✅ | 対応済み（実装・Peer/unitテスト検証済み） |
| 🔧 | 機能追加で対応（ライブラリ本体へprofile/APIの追加が必要） |
| 📝 | exampleのみで対応可能（既存の公開APIの組み合わせで書ける。本体変更不要） |
| ❌ | 対応不可（BLE/NimBLEの範囲外、または対象外と決定済み） |

## BLE基本機能

| 機能 | 状況 | 備考 |
|---|---|---|
| Legacy Advertising | ✅ | name / Service UUID / Manufacturer Data |
| Scanning（active/passive、値型Scan Result） | ✅ | `Gap/Scan`、`Info/ScanDump` |
| Central接続 / Peripheral接続受け入れ / 切断 | ✅ | Scan Result/address直接接続、Connection ID管理 |
| GATT Server（独自Service・Characteristic・Descriptor） | ✅ | 任意UUID、permission、binary-safeな値、Descriptor Write event |
| GATT Client（一覧/既知UUID Discovery・Read・Write） | ✅ | snapshot、Descriptor操作、Write Without Response、操作単位timeout |
| Notify / Indicate（購読・解除・CCCD） | ✅ | `Gatt/NotifyServer`・`Gatt/IndicateServer` |
| MTU交換 / payload上限検証 | ✅ | 接続時snapshot。接続後の再交換は🔧 |
| 複数Serviceの合成（composite） | ✅ | HID+DIS+Battery合成を実装済み |
| Just Works Pairing / Bonding | ✅ | LE Secure Connections |
| 静的passkey / MITM認証 / 暗号化・認証permission | ✅ | `Security/*` example |
| 実行時passkey入力 / Numeric Comparison | 🔧 | stack callbackへの即時応答contextの設計が必要 |
| Privacy（Resolvable Private Address / IRK） | 🔧 | backend対応の確認が必要 |

## HIDプロファイル（USBとの対比が濃い領域）

| 機能 | USB側 | 状況 | 備考 |
|---|---|---|---|
| HID Keyboard Device | ✅ | ✅ | 6KRO / NKRO、LED Output、19 layout連携 |
| HID Keyboard Host | ✅ | ✅ | 6KRO / NKRO parser、usage snapshot、Unicode変換 |
| HID Mouse（Device / Host） | ✅ | ✅ | 相対移動、wheel、5 buttons |
| HID Consumer Control（メディアキー） | ✅ | ✅ | 16-bit usage |
| HID Gamepad | ✅ | ✅ | 6 axis、hat、32 buttons、Host field配送 |
| HID System Control（電源等） | ✅ | ✅ | 8-bit usage |
| 複合HID（keyboard+mouse等） | ✅ | ✅ | 固定Report ID、複数Input Report |
| Vendor HID Report | ✅ | ✅ | 固定ID 6、Input / Output / Feature、Device / Host Peer検証済み |
| 任意Report DescriptorのCustom HID | ✅ | 🔧📝 | 固定Vendor HID以外は🔧、独自GATTで代替するなら📝 |
| NKRO | ✅ | ✅ | EspUsbDevice互換29-byte bitmap、Device / Host Peer検証済み |
| Boot Protocol切替 | ✅ | 🔧 | Report Protocolのみ。Boot characteristic / Protocol Mode切替は未対応 |

## その他プロファイル・サービス

| 機能 | USB側 | 状況 | 備考 |
|---|---|---|---|
| Battery Service | — | ✅ | HID内蔵＋standalone Server/Client example、Peer検証済み |
| Device Information Service（PnP ID等） | Info | ✅ | HID内蔵＋standalone Server/Client example。PnP ID wire形式をPeer検証済み |
| シリアル相当（CDC ACM → Nordic UART Service） | ✅ | 📝 | `Gatt/NusServer` / `NusClient`。packet framingはapplication責務 |
| MIDI（USB MIDI → BLE MIDI） | ✅ | ✅ | BLE MIDI Service。timestamp・running status・複数パケットSysExのcodec（unit test）と`EspBleMidiDevice`/`EspBleMidiHost` profile helper。Device wire形式とHost decodeをPeer検証済み |
| 独自/ベンダーGATTサービス（Vendor bulk相当） | ✅ | ✅ | 既存のGATT Server APIで任意サービスを構築可（`Gatt/Server`） |
| Heart Rate Service | — | ✅ | Body Sensor Location＋可変長MeasurementをServer/Client exampleとPeerで検証済み |
| Environmental Sensing Service | — | ✅ | Temperature / Humidity / PressureのServer/Client exampleとPeer検証済み |
| Health Thermometer Service | — | ✅ | Temperature Type Read＋IEEE-11073 32-bit FLOAT Temperature Measurement Indicate。medical float codec（unit test）とServer/Client example、Peer検証済み |
| Blood Pressure Service | — | ✅ | Feature Read＋IEEE-11073 16-bit SFLOAT（systolic/diastolic/mean）Measurement Indicate。Server/Client example、Peer検証済み |
| Weight Scale Service | — | ✅ | Feature Read＋0.005 kg分解能uint16 Weight Measurement Indicate。Server/Client example、Peer検証済み |
| Body Composition Service | — | ✅ | Feature Read＋uint16 flags・必須Body Fat Percentage（0.1 %/LSB）・任意Weight Measurement Indicate。Server/Client example、Peer検証済み |
| Cycling Speed and Cadence Service | — | ✅ | Feature / Sensor Location Read＋多フィールドCSC Measurement（wheel/crank回転数）Notify。Server/Client example、Peer検証済み |
| Running Speed and Cadence Service | — | ✅ | Feature / Sensor Location Read＋混在幅RSC Measurement（speed/cadence/stride/distance）Notify。Server/Client example、Peer検証済み |
| Cycling Power Service | — | ✅ | Feature / Sensor Location Read＋16bit flags＋符号付き16bit power CP Measurement Notify。Server/Client example、Peer検証済み |
| Pulse Oximeter Service（PLX） | — | ✅ | Features Read＋IEEE-11073 16-bit SFLOAT（SpO2/pulse rate）Spot-Check Measurement Indicate。Server/Client example、Peer検証済み |
| Glucose Service（RACP） | — | ✅ | Record Access Control Point手続き（write→Measurement notify→RACP応答indicate）。sequence/base time/SFLOAT濃度をServer/Client exampleとPeerで検証済み |
| Location and Navigation Service | — | ✅ | LN Feature Read＋flags駆動のLocation and Speed Notify（Instantaneous Speed・sint32緯度経度）。Server/Client example、Peer検証済み |
| User Data Service | — | ✅ | Age・First Nameのread/write、書き込みを`onWritten`で受信しDatabase Change IncrementをNotify。書き込み→onWritten→notifyパスをServer/Client exampleとPeerで検証済み |
| Alert Notification Service | — | ✅ | Supported New Alert Category bitmask Read、Alert Notification Control Point write→category/count/text付きNew Alert Notify。Control Point→notifyパスをServer/Client exampleとPeerで検証済み |
| Immediate Alert Service（Find Me） | — | ✅ | Alert LevelのWrite Without Responseを`onWritten`で受信（Find Meターゲット役）。Write Without ResponseパスをServer/Client exampleとPeerで検証済み |
| Phone Alert Status Service | — | ✅ | Alert Status / Ringer Settingのread/notify、Ringer Control Point（Write Without Response）でSilent Mode切替→Ringer Setting notify。Control Point→状態変更notifyパスをServer/Client exampleとPeerで検証済み |
| Proximity（Link Loss + Tx Power） | — | ✅ | 1 serverにLink Loss Service（Alert Level read/write）とTx Power Service（signed int8 Tx Power Level read）を同居。Server/Client exampleとPeerで検証済み |
| Reference Time Update Service | — | ✅ | Time Update Control Point（Write Without Response）でread専用のTime Update State（Current State＋Result）を遷移。Control Point→state遷移パスをServer/Client exampleとPeerで検証済み |
| Bond Management Service | — | ✅ | Bond Management Feature（uint24 bit field）Read＋Bond Management Control Pointのop code write。Server exampleは切断後に該当bondを削除。GATT choreographyをServer/Client exampleとPeerで検証済み |
| Continuous Glucose Monitoring Service | — | ✅ | E2E-CRC保護のCGM Feature Read＋SFLOAT血糖値/time offset付きCGM Measurement Notify。E2E-CRCは`EspBleCgmCrc.h`（CRC-16/MCRF4XX、unit test）で共有し、Server/Client exampleとPeerで検証済み |
| その他の標準Sensor Service | — | 📝🔧 | 標準UUIDを自分でGATT Server登録すれば📝、profile helperは🔧。IEEE-11073 medical float（SFLOAT含む）は`EspBleMedicalFloat.h`で共有 |
| Current Time Service | — | ✅ | standalone Server/Client example。10-byte wire形式とNotifyをPeer検証済み |
| その他の標準Service | — | 📝 | 標準UUIDのGATT Serverとしてexampleで構築可 |
| OTA / DFU | — | 📝❌ | 独自GATTで自作は📝。統一OTA/DFU方式の提供は対象外❌ |
| Mass Storage（USB MSC相当） | ✅ | ❌ | BLEに実用的な等価がない（帯域・profile不在） |
| USB Audio（UAC相当） | ✅ | ❌ | LE Audioは別スタックで対象外 |
| ネットワーク（CDC-NCM相当） | ✅ | ❌ | BLEのIPSP/6LoWPANは実用外 |
| Hub / トポロジ | ✅ | ❌ | BLEにhub概念なし（複数接続で代替概念） |
| 複数機器同時接続（multiple devices/connections） | ✅ | 🔧 | 接続単位APIは維持。同時接続の実装・検証は🔧 |

## GAP / リンク高度機能

| 機能 | 状況 | 備考 |
|---|---|---|
| Beacon（iBeacon / Eddystone） | 🔧📝 | payloadは`setManufacturerData`等で組めるが、non-connectable指定・Advertising間隔制御は🔧 |
| Extended Advertising / 複数Advertising Set | 🔧 | backend capability依存 |
| Periodic Advertising | 🔧 | backend capability依存 |
| 2M PHY / Coded PHY（Long Range） | 🔧 | backend capability依存 |
| 接続パラメータ更新 / PHY更新 | 🔧 | API未実装 |
| 切断理由の取得 | 🔧 | API未実装 |

## Bluetooth Classic（BR/EDR）— すべて対応不可

EspBleはNimBLE（BLE専用）を使い、初期ターゲットのESP32-S3等はBluetooth Classicを搭載しません。以下はすべて**対応不可**であり、EspBleの責務にも含めません（DECISIONS 対象外）。

| 機能 | 状況 | 備考 |
|---|---|---|
| Bluetooth Classic（BR/EDR）全般 | ❌ | NimBLEはBLEのみ。S3/C3/C6/H2はClassic非搭載 |
| A2DP（オーディオストリーミング） | ❌ | Classicプロファイル |
| HFP（ハンズフリー） | ❌ | Classicプロファイル |
| AVRCP（メディア操作） | ❌ | Classicプロファイル |
| SPP（Serial Port Profile） | ❌ | Classicプロファイル。BLEではNUS等で代替 |
| Classic HID（BT HID） | ❌ | Classicプロファイル。BLEではHOGPで代替 |
| Classic / BLE自動切替（Dual-mode） | ❌ | 対象外 |

## 補足

- 「📝 exampleのみで対応可能」は、`ble.gattServer()`で任意UUIDのService/Characteristicを登録し`notify`/`indicate`できる現状のAPIで書ける、という意味です。標準プロファイルとして正式に対応（profile helper・専用イベント・wire format検証）する場合は🔧になります。
- USB由来機能のうち未対応のもの（任意DescriptorのCustom HID、BLE MIDI、複数接続）は[DECISIONS.ja.md](DECISIONS.ja.md)の優先順位候補と整合しています。採用時にexampleとPeer/unitテストを同時に追加します。
- ❌のうちMSC/Audio/ネットワーク/Mesh/LE Audioは、BLEの技術的範囲外か、別スタック・別ライブラリの領域として対象外です。
