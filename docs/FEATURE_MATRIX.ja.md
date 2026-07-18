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
| Central接続 / Peripheral接続受け入れ / 切断 | ✅ | Connection IDで管理 |
| GATT Server（独自Service・Characteristic） | ✅ | 任意UUIDのService合成可 |
| GATT Client（既知UUID Discovery・Read・Write） | ✅ | 全Service/Characteristic列挙は🔧 |
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
| HID Keyboard Device | ✅ | ✅ | 固定6KRO、LED Output、19 layout連携 |
| HID Keyboard Host | ✅ | ✅ | Report Map parser、usage snapshot、Unicode変換 |
| HID Mouse（Device / Host） | ✅ | ✅ | 相対移動、wheel、5 buttons |
| HID Consumer Control（メディアキー） | ✅ | ✅ | 16-bit usage |
| HID Gamepad | ✅ | ✅ | 6 axis、hat、32 buttons、Host field配送 |
| HID System Control（電源等） | ✅ | ✅ | 8-bit usage |
| 複合HID（keyboard+mouse等） | ✅ | ✅ | 固定Report ID、複数Input Report |
| Vendor / Custom HID Report | ✅ | 🔧📝 | 汎用HIDとしては🔧、独自GATTで代替するなら📝 |
| NKRO / Boot Protocol切替 | ✅ | 🔧 | 現状は固定6KRO Report Protocolのみ |

## その他プロファイル・サービス

| 機能 | USB側 | 状況 | 備考 |
|---|---|---|---|
| Battery Service | — | ✅ | HIDに内蔵。standalone Server/Clientは🔧 |
| Device Information Service（PnP ID等） | Info | ✅🔧 | HID内で一部実装済み。standalone DISは🔧 |
| シリアル相当（CDC ACM → Nordic UART Service） | ✅ | 📝🔧 | 独自UUIDのRX/TX+notifyで📝、正式NUS helperは🔧 |
| MIDI（USB MIDI → BLE MIDI） | ✅ | 🔧 | BLE MIDI Service（timestamp処理）の追加。生データを独自GATTで流すなら📝 |
| 独自/ベンダーGATTサービス（Vendor bulk相当） | ✅ | ✅ | 既存のGATT Server APIで任意サービスを構築可（`Gatt/Server`） |
| センサー系（Environmental Sensing / Heart Rate等） | — | 📝🔧 | 標準UUIDを自分でGATT Server登録すれば📝、profile helperは🔧 |
| Current Time / 各種標準Service | — | 📝 | 標準UUIDのGATT Serverとしてexampleで構築可 |
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
| Address指定のCentral接続 | 🔧 | 現状はScan Result指定のみ |

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
- USB由来機能のうち🔧のもの（HID Mouse/Consumer/Gamepad、BLE MIDI、DIS/NUS、複数接続）は[DECISIONS.ja.md](DECISIONS.ja.md)の優先順位候補と整合しています。採用時にexampleとPeer/unitテストを同時に追加します。
- ❌のうちMSC/Audio/ネットワーク/Mesh/LE Audioは、BLEの技術的範囲外か、別スタック・別ライブラリの領域として対象外です。
