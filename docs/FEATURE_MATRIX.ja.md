# 機能対応マトリクス

EspUsbHost / EspUsbDeviceで扱っている機能のBLE版、およびBLEで一般的に使う機能について、EspBleの対応状況を整理した一覧です。優先順位の確定版は[REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)と[DECISIONS.ja.md](DECISIONS.ja.md)を正とし、この表は俯瞰用のたたき台です。

## 凡例

| 記号 | 意味 |
|---|---|
| ✅ | 対応済み（実装・Peer/unitテスト検証済み） |
| 🔧 | 機能追加で対応（ライブラリ本体へprofile/APIの追加が必要） |
| 📝 | exampleのみで対応可能（既存の公開APIの組み合わせで書ける。本体変更不要） |
| ⚠️ | 部分的に動作する（受信はできるが判別できない等。備考に挙動を明記） |
| ❌ | 対応不可（BLE/NimBLEの範囲外、または対象外と決定済み） |

## BLE基本機能

| 機能 | 状況 | 備考 |
|---|---|---|
| Legacy Advertising | ✅ | name / Service UUID / Manufacturer Data / Service Data / Appearance / Tx Power、connectable/non-connectable、Advertising間隔制御 |
| Scan Response payload | ✅ | `EspBleAdvertising::scanResponse()` がadvertising payloadと同じ builder を返し、31byteをもう1面使える。未設定かつscan response有効時はdevice nameを自動的にこちらへ配置。`scan_response` Peerで passive/active の差を検証済み |
| Scanning（active/passive、値型Scan Result） | ✅ | `Gap/Scan`、`Info/ScanDump`。Scan Resultはaddress / addressType / rssi / connectable / scannable / name / serviceUuids / serviceData（最大4ブロック＋UUID検索） / manufacturerData / appearance / txPowerLevel を保持 |
| Central接続 / Peripheral接続受け入れ / 切断 | ✅ | Scan Result/address直接接続、Connection ID管理、複数同時接続（同時接続数は同梱NimBLE controllerの上限=esp32s3で3）、auto-reconnect（`setAutoReconnect`、既定off） |
| GATT Server（独自Service・Characteristic・Descriptor） | ✅ | 任意UUID、permission、binary-safeな値、Descriptor Write event。登録は`addService()`→`addCharacteristic(service, …)`→`addDescriptor(characteristic, …)`のハンドル連鎖で、以降の値操作・送信・イベントもハンドルで識別する |
| 同一UUIDのService複数登録 | ✅ | **Peripheral側は登録可**（`BLEServiceMap`が`BLEService*`キーのため両方GATTに出る。instance idはEspBleが自動付与）。**Central側も区別可**: discoveryを`ble_gattc_disc_all_svcs()`直呼びで行い、read / write / CCCD書き込みも属性ハンドルへ直接発行するため、2つ目以降のServiceにも到達できる（wrapperの`BLEClient::m_servicesMap`はUUIDキーで2つ目を破棄するので使わない）。Notificationは`BLE_GAP_EVENT_NOTIFY_RX`を値ハンドルで対応付ける。`duplicate_uuid` Peerでread・subscribe・notifyまで検証済み |
| 同一Service内の同一UUID Characteristic | ✅ | **Peripheral側も登録可**: 属性テーブルを`ble_gatts_add_svcs()`で自前に組み、対象はaccess callbackが渡す定義ポインタで識別する（同梱wrapperの`BLEService::addCharacteristic()`は既存を再利用して新しい方をGATTに登録しないため使わない）。**Central側も区別可**（自前discoveryが属性ハンドル単位で列挙し、操作もハンドル指定）。`duplicate_uuid` Peerで、1つのServiceに同一UUIDのCharacteristic 2つ＋同一UUIDのService 2つを公開し、3つすべてへのread・subscribe・notifyを検証済み。wrapper側の問題の報告案は[UPSTREAM_REQUEST_ARDUINO_ESP32_NIMBLE_WHITELIST.ja.md](UPSTREAM_REQUEST_ARDUINO_ESP32_NIMBLE_WHITELIST.ja.md)の補遺 |
| GATT Client（一覧/既知UUID Discovery・Read・Write） | ✅ | 接続ごとのdiscovery snapshot、Descriptor操作、Write Without Response、操作単位timeout、操作の自動キュー。汎用Client操作は同梱wrapperのremoteオブジェクトを使わずNimBLEホストAPIへ直接発行する（UUID重複への対応とdiscoveryのヒープリーク解消のため） |
| Notify / Indicate（購読・解除・CCCD） | ✅ | `Gatt/Basics/NotifyServer`・`Gatt/Basics/IndicateServer`。persistent subscription（`EspBleConfig::persistentSubscriptions`、既定on）で再接続時に自動再購読。復元はpeerアドレスとUUIDで引くため、**UUIDが一意なCharacteristicに限る**（重複時はどれを購読していたか特定できないので記録しない） |
| MTU交換 / payload上限検証 | ✅ | 既定`preferredMtu`は**247**（notify payload 244byte）。Central側は接続成立直後に`ble_gattc_exchange_mtu()`で交換を開始する。結果は`BLE_GAP_EVENT_MTU`で両役割とも追跡し`onMtuChanged`へ配送するため、**`onConnected`時点の`connection.mtu`は両役割とも23**（既定値）である |
| MTUを超える値のRead | ✅ | Characteristic / Descriptor のReadは`ble_gattc_read_long()`で発行し、1 MTUに収まらない値も最後まで読む（HID Report Mapのような数百byteの値が黙って切り詰められない）。短い値は最初の応答で完結するので往復は増えない |
| GATT Serverの読み取りフック | ✅ | `EspBleGattServer::onRead()`。peerがCharacteristicを読んだ瞬間に呼ばれ、その場で`setValue()`した値が返る（センサー値を都度作る用途）。**BLEスタックタスク上で実行される**ため、重い処理を書くとスタックが止まりpeer側はreadがtimeoutする |
| 複数Serviceの合成（composite） | ✅ | HID+DIS+Battery合成を実装済み |
| Just Works Pairing / Bonding | ✅ | LE Secure Connections |
| 静的passkey / MITM認証 / 暗号化・認証permission | ✅ | `Security/*` example |
| 実行時passkey入力 | ✅ | 静的passkeyなしのKeyboardOnly + MITMで、`providePasskey()`によりpairing中に実行時入力。表示側は静的passkeyなしのDisplayOnlyで動的passkey生成→`onPasskeyDisplayed`。応答待ちは30秒で打ち切り。Peer検証済み、`Security/RuntimePasskey{Server,Client}` example |
| Numeric Comparison | ✅ | LE Secure Connections、両側DisplayYesNo + MITM。`onNumericComparison`で比較値を提示し`confirmNumericComparison()`で確認。応答待ちは30秒で打ち切り。Peer検証済み、`Security/NumericComparison{Server,Client}` example |
| Privacy（own address type: Public / Random static / RPA） | ✅ | `EspBleConfig::ownAddressType`。RandomStaticは固定random static、ResolvablePrivateはcontroller回転RPA（`CONFIG_BT_NIMBLE_RPA_TIMEOUT`＝900秒、bonding併用時にpeerがIRKで解決）。`address_privacy` Peerでrandom static advertisingを検証済み |

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
| 任意Report DescriptorのCustom HID | ✅ | ✅ | `ble.hidCustom()`でraw Report Map＋任意Report宣言。内蔵profileと同一HID Serviceに合成。入力/出力を実機Peer検証（同一UUID Reportはclientのhandle指定で撃ち分け）。1デバイス最大4 Report |
| NKRO | ✅ | ✅ | EspUsbDevice互換29-byte bitmap、Device / Host Peer検証済み |
| Boot Protocol切替（Keyboard） | ✅ | ✅ | opt-in（`EspBleHidKeyboardConfig::bootProtocol`、既定off）。Protocol Mode 0x2A4E＋Boot Keyboard Input/Output 0x2A22/0x2A32。Boot Modeで8-byte Boot Reportへ自動切替、`onProtocolMode()`。Mouse Boot Report 0x2A33は未対応 |

## その他プロファイル・サービス

| 機能 | USB側 | 状況 | 備考 |
|---|---|---|---|
| Battery Service | — | ✅ | HID内蔵＋standalone Server/Client example、Peer検証済み |
| Device Information Service（PnP ID等） | Info | ✅ | HID内蔵＋standalone Server/Client example。PnP ID wire形式をPeer検証済み |
| シリアル相当（CDC ACM → Nordic UART Service） | ✅ | 📝 | `Gatt/Basics/NusServer` / `NusClient`。packet framingはapplication責務 |
| MIDI（USB MIDI → BLE MIDI） | ✅ | ✅ | BLE MIDI Service。timestamp・running status・複数パケットSysExのcodec（unit test）と`EspBleMidiDevice`/`EspBleMidiHost` profile helper。Device wire形式とHost decodeをPeer検証済み |
| 独自/ベンダーGATTサービス（Vendor bulk相当） | ✅ | ✅ | 既存のGATT Server APIで任意サービスを構築可（`Gatt/Basics/Server`） |
| Heart Rate Service | — | ✅ | Body Sensor Location＋可変長MeasurementをServer/Client exampleとPeerで検証済み |
| Environmental Sensing Service | — | ✅ | Temperature / Humidity / PressureのServer/Client exampleとPeer検証済み |
| Health Thermometer Service | — | ✅ | Temperature Type Read＋IEEE-11073 32-bit FLOAT Temperature Measurement Indicate。medical float codec（unit test）とServer/Client example、Peer検証済み |
| Blood Pressure Service | — | ✅ | Feature Read＋IEEE-11073 16-bit SFLOAT（systolic/diastolic/mean）Measurement Indicate。Server/Client example、Peer検証済み |
| Weight Scale Service | — | ✅ | Feature Read＋0.005 kg分解能uint16 Weight Measurement Indicate。Server/Client example、Peer検証済み |
| Body Composition Service | — | ✅ | Feature Read＋uint16 flags・必須Body Fat Percentage（0.1 %/LSB）・任意Weight Measurement Indicate。Server/Client example、Peer検証済み |
| Cycling Speed and Cadence Service | — | ✅ | Feature / Sensor Location Read＋多フィールドCSC Measurement（wheel/crank回転数）Notify。Server/Client example、Peer検証済み |
| Running Speed and Cadence Service | — | ✅ | Feature / Sensor Location Read＋混在幅RSC Measurement（speed/cadence/stride/distance）Notify。Server/Client example、Peer検証済み |
| Cycling Power Service | — | ✅ | Feature / Sensor Location Read＋16bit flags＋符号付き16bit power CP Measurement Notify。Server/Client example、Peer検証済み |
| Fitness Machine Service（FTMS） | — | ✅ | Fitness Machine Feature（0x2ACC）Read＋flags駆動のIndoor Bike Data（0x2AD2、speed 0.01 km/h・cadence 0.5/min・符号付きpower W）Notify＋Fitness Machine Control Point（0x2AD9、write+indicate）とFitness Machine Status（0x2ADA）。Request Control / Set Target Power→応答indicate、Set Target Powerは"Target Power Changed" statusもnotify。Server/Client example、Peer（data＋control）検証済み |
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
| 複数機器同時接続（multiple devices/connections） | ✅ | ✅ | 接続ごとのcache・購読・GATT routingで分離。同時接続数の上限は同梱NimBLE controller（esp32s3で3）。3台manual test `multi_connection`で検証済み |

## GAP / リンク高度機能

| 機能 | 状況 | 備考 |
|---|---|---|
| Beacon（non-connectable broadcaster） | ✅ | `setConnectable(false)`＋`setScanResponseEnabled(false)`＋`setInterval()`。payloadは`setManufacturerData`等で構築。実機Peer検証済み |
| iBeacon（Apple beacon layout） | ✅ | backend非依存codec `EspBleIBeacon.h`（`espBleEncodeIBeacon`/`espBleDecodeIBeacon`）。company ID 0x004C＋UUID＋major/minor＋measured power。unit test＋`ibeacon` Peerでbroadcast/decodeを検証済み |
| Advertising Service Data（AD 0x16） | ✅ | `EspBleAdvertising::addServiceData(uuid, data, length)`で最大4ブロック送信、`EspBleScanResult::serviceData[]`/`serviceDataCount`/`serviceDataFor(uuid, data)`で受信。複数ブロックとUUID検索を`service_data` Peerで検証済み |
| Filter Accept List（Peripheral側の接続制限） | ✅ | `EspBle::addToAcceptList()` ＋ `EspBleAdvertising::setFilterPolicy()`（Any / ScanRequest / Connection / Both）。コントローラが弾くのでアプリまで届かない。同梱wrapperのwhite list APIはリンク不能なため`ble_gap_wl_set()`を直接使用（[UPSTREAM_REQUEST_ARDUINO_ESP32_NIMBLE_WHITELIST.ja.md](UPSTREAM_REQUEST_ARDUINO_ESP32_NIMBLE_WHITELIST.ja.md)）。`accept_list` Peerで検証済み |
| Directed Advertising（送信） | ✅ | `EspBleAdvertising::setDirectedTarget(address, addressType, highDuty)` / `clearDirectedTarget()`。`ble_gap_adv_start()`を直接呼ぶ。仕様上ADデータを載せられないためpayloadは送出されず、指定した相手だけが接続できる。highDutyは3.75 ms間隔・最大1.28秒。相手がRPAを使う場合はボンド経由で解決するため先にボンディングが必要。`directed_advertising` Peerで検証済み |
| Directed Advertising（受信） | ⚠️ | 自分宛のADV_DIRECT_INDはスキャン結果として届き、address / addressType / rssi / connectable=true / scannable=false を持つ（仕様上ADデータを載せないため他は空）。そのまま接続可。ただし**advertisement typeを公開していないため判別不能**で、「connectable かつ non-scannable かつ payload空」からの推測になる |
| スキャン側のFilter Accept List | ✅ | `EspBleScanConfig::acceptListOnly`。`ble_gap_disc()` の filter policy に渡すため、許可リスト外のアドバタイズはコントローラが捨てアプリまで届かない。`accept_list` Peerで、リストが空なら1件も報告されず、アドレス追加後は報告されることを検証済み |
| 送信電力の変更 | ✅ | `EspBle::setTxPower(dBm)` / `txPower()`。無線が対応する飛び飛びの値（-12..+9 dBm、3 dB刻み）へ丸める。`local_identity` Peerで、設定値がadvertisingのTx Power Levelとして電波に出ることを検証済み |
| 自分のアドレスの取得 | ✅ | `EspBle::localAddress()` / `localAddressType()`。RPA使用時は回転のたびに変わる現在値。`local_identity` Peerで、相手がスキャンで観測したアドレスと一致することを検証済み |
| 接続時のパラメータ / PHY指定 | 🔧 | `connect()`でConnection IntervalやPHYを指定できない。接続確立後に`updateConnectionParameters()`/`updatePhy()`で変更する |
| 切断理由の指定（送信側） | ✅ | `disconnect(id, reason)`。`disconnectReason`はHCIコードへ正規化して公開する（backendは0x200オフセット付きで報告するため）ので、渡した値がそのまま相手に現れる。`local_identity` Peerで検証済み |
| Advertisingチャネルマップの選択 | ✅ | `EspBleAdvertising::setChannelMap(mask)`（`EspBleAdvertisingChannel37/38/39`のビットマスク、0で3チャネル全部）。Wi-Fiと重なるチャネルを避けられる代わりに、見つかるまでの時間は延びる。`directed_advertising` Peerでチャネル39のみでの接続を検証済み |
| Extended Advertising / 複数Advertising Set | ❌ | 同梱NimBLEが`CONFIG_BT_NIMBLE_EXT_ADV`無効でビルドされており、Arduinoライブラリからは有効化不可 |
| Periodic Advertising | ❌ | Extended Advertising（`CONFIG_BT_NIMBLE_EXT_ADV`）に依存するため同上で対応不可 |
| 2M PHY / Coded PHY（Long Range） | ✅ | 接続後のPHY更新で対応（下記「PHY更新」）。2Mは実機Peer検証済み、Coded（Long Range）は無線対応依存 |
| 接続パラメータ更新 | ✅ | `updateConnectionParameters()`要求＋`onConnectionParametersUpdated()`で結果配送。`EspBleConnection`にinterval/latency/timeoutを公開。両役割・両パスをPeerで検証済み |
| PHY更新（2M / Coded） | ✅ | `updatePhy()`要求＋`onPhyUpdated()`で結果配送。`EspBleConnection`にtx/rx PHYを公開。2M PHYへの更新をPeerで検証済み（Codedは無線対応依存） |
| 切断理由の取得 | ✅ | `EspBleConnection::disconnectReason`（onDisconnectedでbackend/HCI理由コード）。Server/Client両パスをPeerで検証済み |
| GATT Service Changed | ✅ | Server側`notifyServicesChanged()`で0x1801/0x2A05のindication送出、Client側は購読して受信・range decode。Peer検証済み（受信時の自動再Discoveryはアプリ判断） |

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
- USB由来機能のBLE版（任意DescriptorのCustom HID、BLE MIDI、複数同時接続）はいずれも対応済みで、exampleとPeer/unitテストを備えます。今後の未実装候補は[DECISIONS.ja.md](DECISIONS.ja.md)の「優先順位候補」を参照してください。
- ❌のうちMSC/Audio/ネットワーク/Mesh/LE Audioは、BLEの技術的範囲外か、別スタック・別ライブラリの領域として対象外です。
