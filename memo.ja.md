# ESP32向け汎用Bluetooth LEライブラリ 要件定義

> [!IMPORTANT]
> この文書は初期検討のための**たたき台**であり、決定済みの要件や正式な仕様ではありません。
> 内容は検討しながら`docs/`および`tests/TEST_PLAN.ja.md`へ分解・修正して移行します。
> 設計上の決定は`docs/DECISIONS.ja.md`、正式な要件は`docs/REQUIREMENTS.ja.md`を正とします。
> 必要な内容の移行と確認が完了した後、この`memo.ja.md`は削除します。

## 1. 概要

本ライブラリは、ESP32シリーズでNimBLEを利用したBluetooth Low Energy機能を簡単かつ統一的に利用するためのArduino向け汎用ライブラリである。

単一の用途やプロファイルに限定せず、以下を共通基盤上で扱う。

* Bluetooth LE Peripheral
* Bluetooth LE Central
* GATT Server
* GATT Client
* BeaconおよびConnectionless通信
* Bluetooth SIG標準サービス／プロファイル
* ベンダー独自GATTサービス

HIDやMIDIだけを提供する専用ライブラリではなく、USBにおける`EspUsbHost`および`EspUsbDevice`と同様に、Bluetooth LEで一般的に利用される機能を幅広く提供する。

---

## 2. 目的

### 2.1 主目的

NimBLEの低レベルAPIやイベント処理をアプリケーションから分離し、一般的なBluetooth LE機能を一貫したインターフェースで利用できるようにする。

### 2.2 副目的

* プロファイルごとに異なる初期化方法やイベント処理を統一する
* CentralとPeripheralを同一アプリケーション内で併用可能にする
* 複数のGATTサービスを組み合わせて利用可能にする
* 標準サービスと独自サービスを同じ仕組みで扱えるようにする
* ESP32シリーズ間の差異をライブラリ側で吸収する
* Bluetooth LE機器の試作、評価、変換、ゲートウェイ用途に利用できるようにする
* 将来のBluetooth LE機能やプロファイル追加を妨げない構造とする

---

## 3. 対象範囲

### 3.1 対象プラットフォーム

主対象はArduino Core for ESP32とする。

対象候補は以下とする。

* ESP32
* ESP32-C3
* ESP32-C5
* ESP32-C6
* ESP32-H2
* ESP32-S3
* その他、Arduino Core for ESP32とNimBLEを利用可能なESP32シリーズ

チップごとに利用可能なBluetooth LE機能、同時接続数、PHY、Extended Advertisingなどが異なる場合は、機能の有無を明示的に判定できることを要件とする。

### 3.2 Bluetooth方式

対象はBluetooth Low Energyのみとする。

以下は基本対象外とする。

* Bluetooth Classic
* Classic HID
* SPP
* A2DP
* AVRCP
* HFP
* Classic BluetoothとBluetooth LEの自動切り替え

Bluetooth Classicとの両対応は、NimBLEを利用する本ライブラリの責務に含めない。

### 3.3 フレームワーク

初期対象はArduinoとする。

ESP-IDFネイティブ対応は、共通コードを再利用できる構造を考慮するが、初期必須要件には含めない。

---

## 4. 基本設計方針

### 4.1 単一ライブラリ

Central用とPeripheral用にライブラリを分割せず、1つのライブラリで双方を扱う。

理由は以下のとおりである。

* CentralとPeripheralが同じNimBLEホストスタックを共有する
* GATT ClientとGATT Serverを同時に使用する用途がある
* Scanner、Advertiser、Security、Connection管理などの共通機能が多い
* BLE-to-BLEゲートウェイや入力変換装置では双方の役割が必要になる
* 初期化やリソース管理を複数ライブラリが個別に行うと競合しやすい

### 4.2 役割を固定しない

ライブラリ利用開始時に、デバイス全体を「Central」または「Peripheral」のどちらか一方へ固定しない。

アプリケーションは必要に応じて以下を同時利用できることを要件とする。

* Advertising
* Scanning
* Peripheral接続
* Central接続
* GATT Server
* GATT Client

### 4.3 機能単位の構成

ライブラリは、次の機能層から構成されるものとする。

1. Bluetooth LEコア管理
2. GAP機能
3. 接続管理
4. GATT共通機能
5. GATT Server機能
6. GATT Client機能
7. セキュリティ機能
8. 標準サービス／プロファイル
9. 独自サービス作成支援
10. 診断およびデバッグ

### 4.4 プロファイルの組み合わせ

1つのアプリケーション内で複数のサービスまたはプロファイルを併用できることを必須とする。

例：

* HID Keyboard + Battery Service
* HID Gamepad + Device Information Service
* MIDI + 独自設定サービス
* UART相当サービス + OTA管理サービス
* Environmental Sensing Client + 独自転送サービス
* HID Host + HID Device
* センサーCentral + Web設定用Peripheral

プロファイルの利用によって、他のGATTサービス追加が不可能になる構造は避ける。

---

## 5. Bluetooth LEコア要件

### 5.1 初期化と終了

以下を提供すること。

* Bluetooth LEスタックの初期化
* デバイス名の設定
* デバイスアドレス種別の設定
* スタック状態の取得
* 安全な終了または再初期化
* 二重初期化の防止
* ライブラリ内各機能による初期化状態の共有

利用するプロファイルごとに個別のスタック初期化を要求しない。

### 5.2 状態管理

少なくとも以下の状態を取得できること。

* 未初期化
* 初期化中
* 使用可能
* Advertising中
* Scanning中
* 接続中
* エラー状態

### 5.3 機能判定

実行対象のESP32で以下が利用可能か判定できること。

* Bluetooth LE自体
* 2M PHY
* Coded PHY
* Extended Advertising
* Periodic Advertising
* 複数接続
* Central／Peripheral同時動作
* Bluetooth Mesh
* Privacy機能

未対応機能を使用した場合は、コンパイルエラー、明示的な実行時エラー、または機能判定APIのいずれかで判別できること。

---

## 6. GAP要件

## 6.1 Advertising

以下のAdvertising機能を提供すること。

* Connectable Advertising
* Non-connectable Advertising
* Scannable Advertising
* Legacy Advertising
* Advertising開始／停止
* Advertising間隔設定
* Advertisingデータ設定
* Scan Response設定
* Local Name設定
* Service UUID掲載
* Manufacturer Specific Data掲載
* Service Data掲載
* TX Power掲載
* Advertising終了通知

対応ハードウェアでは、以下を追加対象とする。

* Extended Advertising
* 複数Advertising Set
* Periodic Advertising

### 6.2 Scanning

以下を提供すること。

* Active Scan
* Passive Scan
* スキャン開始／停止
* スキャン時間設定
* スキャン間隔／Window設定
* Duplicate Filter設定
* RSSI取得
* デバイスアドレス取得
* デバイス名取得
* Service UUID取得
* Manufacturer Specific Data取得
* Service Data取得
* Connectableかどうかの判定
* スキャン結果コールバック
* スキャン結果一覧取得

以下の条件で検索できること。

* デバイスアドレス
* デバイス名
* 名前の前方一致
* Service UUID
* Manufacturer ID
* Manufacturer Data
* Service Data
* RSSI
* Connectable属性

### 6.3 Beacon

Connectionless用途として、少なくとも以下を扱えること。

* 任意Advertisingデータの送信
* 任意Advertisingデータの受信
* iBeacon形式
* Eddystone UID
* Eddystone URL
* Eddystone TLM

特定のBeacon形式に限定せず、ユーザー定義フォーマットも扱えること。

---

## 7. 接続管理要件

### 7.1 共通接続管理

Central起点とPeripheral起点を問わず、接続を統一的に管理できること。

各接続について以下を取得できること。

* Connection Handle
* Peer Address
* Local Role
* RSSI
* MTU
* 接続間隔
* Slave Latency
* Supervision Timeout
* 暗号化状態
* 認証状態
* Bond状態
* 使用中のPHY

### 7.2 接続操作

以下を提供すること。

* スキャン結果からの接続
* アドレスを指定した接続
* 接続タイムアウト
* 切断
* 切断理由取得
* 接続パラメータ更新要求
* PHY更新要求
* MTU交換要求
* RSSI更新
* 自動再接続
* 再接続条件設定

### 7.3 複数接続

複数のBluetooth LE機器との同時接続を考慮する。

要件は以下とする。

* 接続をオブジェクトまたは識別子単位で管理できる
* グローバルな「現在の接続」だけに依存しない
* 接続ごとにClient操作を実行できる
* 接続ごとに購読状態を管理できる
* 接続ごとにセキュリティ状態を管理できる
* 最大接続数を取得できる
* 最大接続数を超えた場合に明示的なエラーを返す

初期バージョンで複数接続を完全対応しない場合でも、APIが単一接続前提で固定されないことを要件とする。

---

## 8. GATT共通要件

### 8.1 UUID

以下のUUIDを扱えること。

* Bluetooth SIG 16-bit UUID
* 32-bit UUID
* 128-bit UUID
* 文字列形式UUID
* バイナリ形式UUID

UUIDの比較、文字列化、標準UUID名の取得を提供対象とする。

### 8.2 データ型

Characteristic値は、少なくとも以下の形式で扱えること。

* バイト列
* 文字列
* 整数
* 浮動小数点数
* 構造化データ
* ユーザー定義エンコード

内部表現を特定の文字列型だけに限定しない。

### 8.3 ATT MTU

以下を要件とする。

* MTUの設定
* 接続ごとのネゴシエート結果取得
* MTUを超えるデータの検出
* Long Read／Long Writeへの対応方針を明示
* 分割送信が必要な場合の支援

### 8.4 エラー

以下のエラー種別を区別できること。

* Hostエラー
* HCIエラー
* ATTエラー
* L2CAPエラー
* Security Managerエラー
* タイムアウト
* 未接続
* 未発見
* 未対応
* リソース不足
* 引数不正
* 状態不正

単なる`true`／`false`だけでなく、原因を確認可能な結果を返すこと。

---

## 9. GATT Server要件

### 9.1 サービス定義

以下を定義できること。

* Primary Service
* Secondary Service
* Included Service
* Characteristic
* Descriptor
* Characteristic User Description
* Client Characteristic Configuration Descriptor
* Presentation Format
* 独自Descriptor

### 9.2 Characteristic属性

以下を指定できること。

* Read
* Write
* Write Without Response
* Notify
* Indicate
* Broadcast
* Signed Write
* 暗号化必須
* 認証必須
* Authorization必須

### 9.3 値の管理

以下の方式を選択できること。

* ライブラリ内部に値を保持する方式
* Read時にコールバックで値を生成する方式
* Write時にコールバックで処理する方式
* アプリケーション所有バッファを参照する方式

### 9.4 Notify／Indicate

以下を提供すること。

* 特定接続へのNotify
* 全購読接続へのNotify
* 特定接続へのIndicate
* 購読状態の取得
* CCCD変更通知
* 送信完了通知
* 送信失敗通知
* 接続ごとの購読管理

### 9.5 サービス合成

複数の標準サービスおよび独自サービスを同時登録できること。

各プロファイルがGATT Server全体を独占しないこと。

---

## 10. GATT Client要件

### 10.1 Discovery

以下を提供すること。

* 全Service Discovery
* UUID指定Service Discovery
* Characteristic Discovery
* Descriptor Discovery
* Service Changedへの対応方針
* Discovery結果のキャッシュ
* キャッシュの破棄
* 再Discovery

### 10.2 Read／Write

以下を提供すること。

* Characteristic Read
* Descriptor Read
* Characteristic Write
* Write Without Response
* Descriptor Write
* Long Read
* Long Write
* 応答待ち
* タイムアウト
* 非同期完了通知

### 10.3 Subscribe

以下を提供すること。

* Notify購読
* Indicate購読
* 購読解除
* 受信コールバック
* CCCDの自動設定
* 再接続時の再購読方針

### 10.4 汎用Client

対応済みプロファイル以外でも、UUIDを指定して任意のGATTサービスを操作できること。

プロファイルクラスを使用しなくても、汎用GATT Clientとして利用可能にする。

---

## 11. セキュリティ要件

### 11.1 Pairing

以下を扱えること。

* Just Works
* Passkey Entry
* Numeric Comparison
* Out-of-Bandの対応可否明示
* Pairing開始
* Pairing要求通知
* Pairing完了通知
* Pairing失敗理由取得

### 11.2 I/O Capability

以下のデバイス能力を指定できること。

* Display Only
* Display Yes/No
* Keyboard Only
* No Input No Output
* Keyboard Display

### 11.3 Bonding

以下を提供すること。

* Bonding有効／無効
* Bond済み機器一覧
* 特定Bond情報削除
* 全Bond情報削除
* Bond済み機器かどうかの判定
* Bond数上限の取得
* Bond情報満杯時の通知

### 11.4 暗号化と認証

以下を設定できること。

* Encryption要求
* Authentication要求
* MITM Protection要求
* Secure Connections要求
* Key Size条件

プロファイル単位またはCharacteristic単位で必要なセキュリティ条件を設定できること。

### 11.5 Privacy

対応環境では以下を対象とする。

* Resolvable Private Address
* Identity Resolving Key
* Privacy有効／無効
* Peer Identityの取得

---

## 12. 標準サービス／プロファイル要件

すべてのBluetooth SIGプロファイルを一度に実装するのではなく、利用頻度と汎用性に基づいて段階的に対応する。

## 12.1 第1優先グループ

初期の主要対応候補とする。

### Human Interface Device

Server／Device側：

* Keyboard
* Mouse
* Consumer Control
* Gamepad
* 複合HID
* Boot Protocol対応可否の明示
* Report Mapのカスタマイズ

Client／Host側：

* HID Service Discovery
* Report Discovery
* Input Report受信
* Output Report送信
* Feature Report操作
* Keyboard
* Mouse
* Consumer Control
* Gamepad
* ベンダー独自Report

HID Clientは特定の固定Report Mapだけに限定しない。

### MIDI

* MIDI Server
* MIDI Client
* Note On／Off
* Control Change
* Program Change
* Pitch Bend
* SysEx
* Timestamp
* Raw MIDI Packet
* 複数メッセージ処理

### Battery Service

* Battery Level Server
* Battery Level Client
* Notify
* 複数Battery Instanceを考慮

### Device Information Service

以下の標準情報を扱う。

* Manufacturer Name
* Model Number
* Serial Number
* Hardware Revision
* Firmware Revision
* Software Revision
* System ID
* PnP ID

### UART相当サービス

Bluetooth SIG標準UARTプロファイルは存在しないため、以下を個別アダプタとして扱う。

* Nordic UART Service互換
* 任意RX／TX Characteristic構成
* Raw Stream
* Line単位受信
* Packet単位受信

ライブラリ固有のUARTサービスだけに固定しない。

## 12.2 第2優先グループ

### Generic Sensor系

* Environmental Sensing Service
* Temperature
* Humidity
* Pressure
* Light
* Generic Sensor Data

### Health系の基本サービス

* Heart Rate Service
* Health Thermometer Service
* Blood Pressure Service
* Pulse Oximeter Service

医療機器としての認証や診断用途は対象外とし、通信プロファイルの利用支援に限定する。

### Cycling／Fitness系

* Cycling Speed and Cadence
* Cycling Power
* Running Speed and Cadence
* Fitness Machine Service

### Current Time系

* Current Time Service
* Reference Time Update Service
* Next DST Change Service

### Location系

* Location and Navigation Service

## 12.3 第3優先グループ

必要性と検証機器の入手性を確認して対応する。

* Alert Notification Service
* Immediate Alert Service
* Link Loss Service
* Phone Alert Status Service
* Tx Power Service
* Proximity Profile
* Object Transfer Service
* Internet Protocol Support Service
* HTTP Proxy Service
* User Data Service
* Weight Scale Service
* Glucose Service
* Continuous Glucose Monitoring Service

## 12.4 別モジュール候補

規模や性質が大きく異なるため、コアライブラリへの直接包含を必須としない。

* Bluetooth Mesh
* LE Audio
* Electronic Shelf Label
* Matter向けBluetooth LE Provisioning
* DFU／OTAのベンダー固有方式
* Apple Notification Center Service
* Apple Media Service
* Fast Pair
* ベンダー固有スマートウォッチ連携

これらは拡張パッケージまたは別ライブラリとして提供可能な構造とする。

---

## 13. 独自サービス要件

標準プロファイルへの対応だけでなく、独自GATTサービスを容易に作成できることを必須とする。

以下を提供対象とする。

* UUID指定によるService作成
* Characteristic追加
* Descriptor追加
* Property設定
* Permission設定
* Read／Writeコールバック
* Notify／Indicate
* Client側Service Wrapper作成支援
* Server側Service Wrapper作成支援
* 標準サービスとの同時利用

独自サービスを利用するために、ライブラリ内部コードを変更する必要がないこと。

---

## 14. プロファイル共通インターフェース要件

各プロファイルは可能な範囲で共通したライフサイクルを持つこと。

概念上、以下の操作を統一する。

* 登録
* 開始
* 停止
* 接続
* 切断
* Discovery
* 利用可能状態の取得
* エラー取得
* イベント登録

ただし、すべてのプロファイルを不自然に同一APIへ押し込めない。

共通部分は統一し、プロファイル固有のデータや操作は個別APIとして提供する。

---

## 15. イベント要件

### 15.1 共通イベント

少なくとも以下を通知できること。

* Stack Ready
* Advertising Started
* Advertising Stopped
* Scan Started
* Scan Result
* Scan Completed
* Connected
* Disconnected
* Connection Updated
* MTU Changed
* PHY Changed
* Security Changed
* Pairing Requested
* Pairing Completed
* Pairing Failed
* Bond Changed
* GATT Error

### 15.2 イベント提供方式

Arduino利用者向けに、以下のいずれかまたは複数を提供可能とする。

* Callback
* Listener
* Event Queue
* Polling
* 状態取得

ライブラリ内部タスクのコンテキストで重いユーザー処理を直接実行することを強制しない。

### 15.3 イベント情報

イベントには可能な限り以下を含める。

* イベント種別
* Connection識別子
* Peer情報
* 対象Service／Characteristic
* データ
* 結果コード
* エラー理由
* Timestamp

---

## 16. 同期／非同期要件

Bluetooth LE処理は本質的に非同期であるため、非同期操作を基本とする。

ただし、Arduinoスケッチで利用しやすいように、必要に応じて以下の双方を提供可能とする。

* 非同期API
* タイムアウト付き同期ラッパー

同期APIは無期限にブロックしない。

コールバック内部から同期APIを呼び出した際のデッドロックを防止するか、禁止事項として明示する。

---

## 17. リソース管理要件

### 17.1 メモリ

以下を確認可能にする。

* 接続数
* 登録サービス数
* 登録Characteristic数
* Discoveryキャッシュ使用量
* 送受信キュー使用量
* リソース不足エラー

### 17.2 動的機能追加

初期化後にサービスやプロファイルを追加できるかどうかを明確にする。

NimBLEまたはGATTデータベース上の制約によって動的追加が困難な場合は、以下の方式を採用できること。

* 初期化前に構成を登録
* 構成確定後にスタック開始
* 再構成時は安全に再初期化

### 17.3 オブジェクト寿命

以下を明確にする。

* ライブラリ本体の寿命
* Connectionオブジェクトの寿命
* Serviceオブジェクトの寿命
* Characteristicオブジェクトの寿命
* Scan Resultの寿命
* Callbackに渡されるデータの寿命

---

## 18. エラー処理要件

### 18.1 エラー情報

エラーは以下の情報を提供する。

* ライブラリ共通エラーコード
* 元のNimBLEエラーコード
* エラー分類
* 人間が確認可能な説明
* 対象操作
* 対象接続

### 18.2 致命的でないエラー

以下の状況でアプリケーション全体を停止させない。

* 接続失敗
* Discovery失敗
* Pairing失敗
* Notify失敗
* 一時的なリソース不足
* Peerによる切断
* Service未発見
* Characteristic未発見

### 18.3 復旧

以下を考慮する。

* 再スキャン
* 再接続
* 再Discovery
* 再購読
* Advertising再開
* Stack再初期化

---

## 19. ログおよび診断要件

以下のログレベルを設ける。

* Error
* Warning
* Information
* Debug
* Trace

診断情報として以下を出力可能にする。

* Advertising内容
* Scan Result内容
* 接続パラメータ
* Security状態
* GATT Service一覧
* Characteristic一覧
* Descriptor一覧
* MTU
* Notify／Indicate内容
* NimBLEエラーコード
* 切断理由

ログ出力先は`Serial`固定にせず、無効化または差し替え可能にする。

---

## 20. API品質要件

### 20.1 Arduino利用者向け

単純な用途では少ないコードで利用できること。

例：

* Beaconを送信する
* デバイスをスキャンする
* Battery Serviceを公開する
* HID Keyboardとして動作する
* MIDIメッセージを送信する
* Nordic UART Serviceへ接続する

### 20.2 上級利用者向け

以下を隠蔽しすぎないこと。

* UUID
* Raw Advertising Data
* Characteristic Property
* Security Permission
* Connection Parameter
* MTU
* PHY
* Raw GATT操作
* 元のNimBLEエラー

### 20.3 低レベルアクセス

必要に応じて、NimBLEのネイティブオブジェクトまたはハンドルへアクセスできる逃げ道を提供する。

ただし、低レベルアクセスを通常利用の必須条件にはしない。

---

## 21. 互換性要件

### 21.1 バージョン互換性

以下の変化を考慮する。

* Arduino Core for ESP32の更新
* ESP-IDFの更新
* NimBLEの更新
* 対象ESP32チップの追加
* Bluetooth Core仕様の追加

### 21.2 API互換性

公開APIはSemantic Versioningに従う。

* Patch：バグ修正
* Minor：後方互換な機能追加
* Major：互換性を壊す変更

### 21.3 プロファイル互換性

標準プロファイルはBluetooth SIG仕様に基づく。

独自拡張を標準UUIDや標準データ形式へ混入させない。

---

## 22. サンプル要件

少なくとも次のサンプルを用意する。

### 基本

* Scan
* Advertise
* Peripheral
* Central
* GATT Server
* GATT Client
* Security
* Multiple Connections
* Central and Peripheral Concurrent

### 標準サービス

* Battery Server
* Battery Client
* Device Information Server
* Environmental Sensor Server
* Environmental Sensor Client

### HID

* Keyboard Device
* Mouse Device
* Gamepad Device
* Composite HID Device
* Keyboard Host
* Mouse Host
* Gamepad Host

### MIDI

* MIDI Device
* MIDI Client
* MIDI Loopback

### 独自通信

* UART-like Server
* UART-like Client
* Custom Service Server
* Custom Service Client

### 複合用途

* HID + Battery
* MIDI + Device Information
* Sensor Central + Configuration Peripheral
* HID Host to HID Device Bridge

---

## 23. テスト要件

### 23.1 単体テスト

以下を対象とする。

* UUID処理
* Advertisingデータ生成／解析
* GATTデータエンコード／デコード
* プロファイルデータ変換
* エラー変換
* 状態遷移
* Discovery結果処理

### 23.2 実機テスト

以下を検証する。

* Android
* iOS
* Windows
* Linux
* macOS
* ESP32同士
* 市販BLEキーボード
* 市販BLEマウス
* 市販BLEゲームパッド
* BLE MIDI機器
* BLEセンサー
* nRF Connect相当の汎用ツール

### 23.3 相互運用性

特定OSまたは特定アプリだけで動作する実装を避ける。

可能な限り、複数のCentral／Peripheral実装との相互接続を検証する。

---

## 24. 非機能要件

### 24.1 安定性

* 長時間接続で動作する
* 接続／切断を繰り返してもリソースリークしない
* 存在しない機器への接続失敗から復旧できる
* 不正なGATTデータを受信してもクラッシュしない
* 通信相手が突然消失しても復旧できる

### 24.2 性能

* センサー通知やHID入力に十分な遅延で処理できる
* Write Without Responseによる連続送信を妨げない
* MTU拡張を利用可能にする
* 不要なデータコピーを抑える
* プロファイル未使用時に不要コードを含めない構成を考慮する

### 24.3 サイズ

機能を選択的に組み込めることを推奨要件とする。

すべてのプロファイルを使用しないアプリケーションに、全プロファイルのコードやテーブルを強制的に含めない。

### 24.4 可読性

標準Bluetooth用語を使用する。

独自に「Host」「Device」とだけ表現すると、HCI Host、GATT Client、Centralなどと混同しやすいため、APIおよび文書では以下を明確に区別する。

* Host Stack
* Central
* Peripheral
* GATT Client
* GATT Server
* HID Host
* HID Device

---

## 25. 対象外

初期のコアライブラリでは、以下を対象外とする。

* Bluetooth Classic
* ClassicとBLEの統合API
* A2DP
* SPP
* HFP
* AVRCP
* LE Audio
* Bluetooth認証取得代行
* 医療機器としての規格適合保証
* OS固有のBLEバグの完全吸収
* あらゆるベンダー独自プロファイルへの標準対応
* Bluetooth MeshのコアAPIへの直接統合
* Wi-Fi Provisioning方式の統一
* アプリケーション固有データモデルの規定

---

## 26. 初期リリース範囲

最初の安定リリースでは、範囲を次のように限定することを推奨する。

### 必須

* NimBLE初期化管理
* Advertising
* Scanning
* Central接続
* Peripheral接続
* 接続管理
* GATT Server
* GATT Client
* Read／Write
* Notify／Indicate
* Service Discovery
* Security／Bonding
* 任意独自サービス
* Battery Service
* Device Information Service
* HID Device
* HID Host
* MIDI Device
* MIDI Client
* Nordic UART Service互換
* エラー情報
* ログ
* 基本サンプル

### 次期対応

* Environmental Sensing
* Heart Rate
* Cycling／Fitness
* Extended Advertising
* Periodic Advertising
* Privacy詳細機能
* 複数接続の高度な管理
* プロファイルDiscoveryキャッシュ
* 自動再接続／再購読ポリシー

### 別パッケージ候補

* Mesh
* LE Audio
* OTA／DFU方式
* Apple独自サービス
* Google独自サービス
* 大規模なHealth／Fitnessプロファイル群

---

## 27. 成功条件

本ライブラリは、次の状態を満たしたとき目的を達成したと判断する。

1. 利用者がNimBLEの低レベルイベント処理を直接記述せずに、一般的なBLE機能を利用できる
2. CentralとPeripheralを同じライブラリで使用できる
3. GATT ClientとGATT Serverを同時利用できる
4. 複数の標準サービスを組み合わせられる
5. HID、MIDI、センサー、UART相当通信を共通基盤上で扱える
6. 未対応プロファイルでも汎用GATT APIでアクセスできる
7. 独自サービスをライブラリ改造なしで追加できる
8. プロファイル追加によってコアAPIを大きく変更する必要がない
9. エラーや接続状態をアプリケーションから確認できる
10. ESP32シリーズ間の機能差を判定できる
11. 単純な用途と高度な用途の双方に対応できる
12. Bluetooth Classicを含めないことで責務が明確に保たれている
