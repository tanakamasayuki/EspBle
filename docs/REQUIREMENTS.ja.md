# 要件

## 目的

`EspBle`は、ESP32 ArduinoでBluetooth Low Energyを利用するための汎用ライブラリです。Arduino-ESP32同梱BLE APIの低レベルな初期化、イベント、接続、GATT処理を共通化し、単純なスケッチと複合的なBLEゲートウェイの双方から利用できる基盤を提供します。

USBにおける`EspUsbHost` / `EspUsbDevice`に相当する機能領域を目標にしますが、機能を一括実装せず、汎用基盤を先に固定してプロファイルを優先順位順に追加します。代表的な利用者は`ESP32KeyBridge`のBLE HID input / output adapterです。

## 対象環境

- Arduino framework
- Arduino-ESP32 3.x
- Arduino-ESP32に同梱されたBLEライブラリのNimBLE backend
- Arduino-ESP32がNimBLEを利用可能な構成として提供するESP32

外部のNimBLE-Arduinoを必須依存にせず、BLEスタックを本ライブラリへ複製・同梱しません。Bluedroid backendとBluetooth Classicは対応対象に含めません。検証済みArduino-ESP32バージョンはテストの`sketch.yaml`で固定し、対応範囲は実装開始時に確定します。

初期実機ターゲットはESP32-S3です。対象可否はSoCがBluetooth LEを内蔵しているかだけでは決めず、Arduino-ESP32がそのボード構成でNimBLE backendを提供するかで判断します。

ESP32-P4はBluetooth LEを内蔵しませんが、Arduino-ESP32が規定するGPIOへESP32-C6などの対応コントローラを接続したHosted BLE構成では、Arduinoスケッチから内蔵構成と同じBLE APIを利用できます。この構成もEspBleの対象候補です。P4 + C6 Hosted BLEは初期の常設Peer環境には含めず、build-only testと専用実機確認を追加してから対応済みとします。

内蔵/Hostedの差、接続可能数、PHY、Advertising機能などはbackend capabilityとして扱い、SoC名だけで利用可能機能を推測しません。

## 基本要件

公開API、文書、examplesの用語は[TERMINOLOGY.ja.md](TERMINOLOGY.ja.md)に従います。

### 単一スタックと役割の共存

- Central用とPeripheral用にライブラリを分割しない。
- Advertising、Scanning、Central接続、Peripheral接続、GATT Client、GATT Serverを同じライブラリで構成できる。
- ライブラリ内のすべての機能がスタック初期化とSecurity/Bond情報を共有する。
- 役割をデバイス全体の単一モードとして固定しない。
- 接続はlocal roleを持つConnection単位で管理する。

### 構成の合成

- 複数の標準Serviceと独自Serviceを同じGATT Serverへ登録できる。
- プロファイルがAdvertisingまたはGATT Server全体を独占しない。
- 未対応プロファイルも汎用GATT APIで操作できる。
- `ESP32KeyBridge`など上位ライブラリは、EspBleの所有権を奪わず参照して利用できる。

### エラーと診断

- 公開操作は単なる`bool`だけでなく、失敗分類と元のスタックエラーを取得可能にする。
- 接続失敗、Discovery失敗、Pairing失敗、Peer切断を通常の回復可能な結果として扱う。
- ログ出力先を`Serial`に固定しない。
- Advertising内容、接続状態、GATT Discovery、Security状態、切断理由を診断できる。

## 初期機能範囲

### GAP

- Legacy Advertisingの開始・停止
- Connectable / non-connectable / scannable設定
- Advertising DataとScan Responseの構築
- Local Name、Service UUID、Manufacturer Data、Service Data
- Active / Passive Scan
- Scan interval / window / duration / duplicate filter
- Scan Result callbackと検索
- Address、name、RSSI、Service UUID、Manufacturer Dataの取得

### Connection

- Scan ResultまたはaddressからのCentral接続
- Peripheralとして受け入れた接続の管理
- 接続タイムアウトと切断
- Connection handle、peer address、local role、MTU、Security状態
- 切断理由
- MTU交換と接続パラメータ更新の基本操作
- 接続ごとのGATT Clientと購読状態

初期実装の同時接続数は制限してよいものとしますが、公開APIをグローバルな「現在の接続」だけに固定しません。

### GATT Server

- Primary Service
- CharacteristicとDescriptor
- Read / Write / Write Without Response / Notify / Indicate property
- Read/Write callback
- Characteristic値のbyte sequenceとしての設定と取得
- 接続ごとの購読状態
- 特定接続または全購読接続へのNotify/Indicate
- 複数Serviceの合成

整数や構造体などの型変換はcodec/helperの責務とし、GATTコアの値表現はbyte sequenceを基本にします。

### GATT Client

- Service / Characteristic / Descriptor Discovery
- UUID指定Discovery
- Characteristic / Descriptor Read
- Write / Write Without Response
- Notify / Indicateの購読と解除
- Discovery結果の接続単位管理
- タイムアウト付き操作

Long Read / Long Writeと永続Discovery cacheは、基盤APIを壊さず追加できる構造だけを初期要件とし、初期実装必須にはしません。

### GATT共通

- UUIDは16-bit / 32-bit / 128-bitを文字列表記で受理し、比較時はBluetooth base UUIDへの展開を吸収する。
- 標準UUID名の解決（"1812" → "Human Interface Device"等）は初期要件に含めない。

### Security

- Just Worksを含む基本Pairing
- Bondingの有効化
- Pairing成功・失敗イベント
- 暗号化済み/認証済み状態の取得
- Bond一覧、特定Bond削除、全Bond削除
- 暗号化を要求するCharacteristic

静的passkey、MITM、Secure Connectionsは実装済みです。実行時passkey入力、Numeric Comparison、Out-of-Band、IO Capabilityの拡張（DisplayYesNo / KeyboardDisplay）、Bond上限の取得と満杯通知、Privacyの詳細は将来拡張とします。

### 初期プロファイル

- HID Keyboard Device（BLE Peripheral / GATT Server）
- HID Keyboard Host（BLE Central / GATT Client）
- HID Output Report（keyboard LED）
- Battery Service Server/Clientの基本操作

HID Keyboard HostはReport MapとReport ReferenceをDiscoveryし、固定された特定keyboardだけに依存しない構造とします。初期対応するReport Mapの範囲はHID仕様書で別途固定します。

## 優先順位付き拡張

初期基盤の完成後、需要、実装量、Peerテスト可能性を確認して1機能ずつ追加します。

1. HID Mouse、Consumer Control、複合HID、Boot Protocol
2. Device Information Service、Nordic UART Service互換
3. BLE MIDI
4. 複数接続、自動再接続/再購読、Discovery cache、Service Changed対応
5. Sensor系標準Service
6. Extended / Periodic Advertising、2M / Coded PHY、Privacy詳細
7. Beacon / Connectionless通信（任意Advertisingデータの送受信、iBeacon、Eddystone）
8. GAP詳細設定（Advertising間隔、TX Power掲載、Service Data掲載と検索、アドレス種別設定）
9. Connection詳細（接続中RSSI、接続間隔/Slave Latency/Supervision Timeoutの取得、PHY取得/更新）
10. GATT Server詳細（Secondary/Included Service、User Description等の標準Descriptor、Read時callback、Broadcast/Signed Write property）
11. 診断の拡張（機能判定API、リソース使用量getter、ログレベル体系（Error〜Trace）と出力先差し替え）

この一覧は実装約束ではなく優先順位候補です。各機能は`DECISIONS.ja.md`で採用してから正式要件へ移し、採用時にexampleとPeer/unitテストを同時に追加します。

## 非機能要件

- 長時間接続と接続/切断の反復でheap・task・Bond storeをリークしない（Peerテストで反復検証する）。
- 不正なGATTデータや異常長のHID reportを受信してもクラッシュせず、観測可能なカウンタまたはエラーとして扱う。
- 存在しない機器への接続失敗、peer消失、Pairing失敗から回復できる。
- HID入力とNotificationを実用的な遅延で処理し、Write Without Responseの連続送信を妨げない。
- 未使用プロファイルのコード・テーブルをリンク結果へ強制的に含めない構成を推奨要件とする。

## バージョニング

公開APIはSemantic Versioningに従います（Patch=バグ修正、Minor=後方互換な追加、Major=非互換変更）。1.0.0より前の0.x系は初回リリースまでの試行段階で、互換性を保証しません。

## 対象外

- Bluetooth ClassicとClassic/BLE統合API
- A2DP、SPP、HFP、AVRCP
- LE Audio
- Bluetooth Mesh
- Matter provisioning
- OTA/DFU方式の統一
- Apple/Google固有サービスの標準搭載
- 医療機器としての適合保証
- OS固有問題の完全な吸収
- ESP-IDFネイティブ公開API
- BLEスタックのforkまたは再配布

## 成功条件

- CentralとPeripheralを同一スケッチで構成できる。
- GATT ClientとServerを同時利用できる。
- 複数Serviceを合成できる。
- 任意UUIDのServiceをライブラリ改造なしで扱える。
- Connection、エラー、Security状態をアプリケーションから確認できる。
- ESP32-S3 2台のPeerテストで主要な接続状態遷移を再現可能に検証できる。
- HID Keyboard Host/Deviceを`ESP32KeyBridge`のadapterから利用できる。
