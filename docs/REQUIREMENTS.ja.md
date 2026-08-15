# 要件

何を作るライブラリかを定める文書。対応状況は[FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)、設計判断とその理由は[DECISIONS.ja.md](DECISIONS.ja.md)が正本。

## 目的

`EspBle`は、ESP32 ArduinoでBluetooth Low Energyを利用するための汎用ライブラリ。NimBLE hostの低レベルな初期化、イベント、接続、GATT処理を共通化し、単純なスケッチと複合的なBLEゲートウェイの双方から使える基盤を提供する。無印ESP32ではBluetooth Classicも同じ形のAPIで扱う。

USBにおける`EspUsbHost` / `EspUsbDevice`に相当する機能領域を目標とするが、機能を一括実装せず、汎用基盤を先に固定してプロファイルを追加していく。代表的な利用者は`ESP32KeyBridge`のBLE HID input / output adapter。

## 対象環境

- Arduino framework / Arduino-ESP32 3.x
- NimBLE host。外部NimBLE-Arduinoは必須依存にしない。core同梱NimBLEがある構成ではそれを使い、**同梱が無い無印ESP32でだけEspBleがNimBLE hostを持ち込む**（`src/nimble_esp32/`。coreのプリビルドがBluedroid固定のため）
- 上記のNimBLE hostが動くESP32

対象可否はSoCがBLEを内蔵しているかでは決めず、**その構成でNimBLE hostを動かせるか**で判断する。ESP32-P4はBLEを内蔵しないが、規定のGPIOへESP32-C6などを接続したHosted BLE構成で同じBLE APIが使えるため対応済みとする（1.1.0。Securityなど上流由来の制限は[既知制限](ESP_HOSTED_LIMITATIONS.ja.md)に記録する）。無印ESP32はBluetooth Classicも持つため、BLEとは別に名前空間化したClassic-only Bluedroid hostをarchiveとして同梱する。**このchip以外の生成物は変わらない。**

内蔵/Hostedの差、接続可能数、PHY、Advertising機能はbackend capabilityとして扱い、**SoC名だけで利用可能機能を推測しない**。検証済みArduino-ESP32バージョンはテストの`sketch.yaml`で固定する。

## 基本要件

用語は[TERMINOLOGY.ja.md](TERMINOLOGY.ja.md)に従う。

### 単一スタックと役割の共存

- Central用とPeripheral用にライブラリを分割しない。
- Advertising、Scanning、Central接続、Peripheral接続、GATT Client、GATT Serverを同じライブラリで構成できる。
- すべての機能がスタック初期化とSecurity/Bond情報を共有する。
- 役割をデバイス全体の単一モードとして固定せず、接続はlocal roleを持つConnection単位で管理する。

### 構成の合成

- 複数の標準Serviceと独自Serviceを同じGATT Serverへ登録できる。
- プロファイルがAdvertisingまたはGATT Server全体を独占しない。
- 未対応プロファイルも汎用GATT APIで操作できる。
- `ESP32KeyBridge`など上位ライブラリは、EspBleの所有権を奪わず参照して使える。

### エラーと診断

- 公開操作は単なる`bool`だけでなく、失敗分類と元のスタックエラーを取得できる。
- 接続失敗、Discovery失敗、Pairing失敗、Peer切断を**通常の回復可能な結果**として扱う。
- ログ出力先を`Serial`に固定しない。
- Advertising内容、接続状態、GATT Discovery、Security状態、切断理由を診断できる。

## 非機能要件

- 長時間接続と接続/切断の反復でheap・task・Bond storeをリークしない（Peerテストで反復検証する）。
- 不正なGATTデータや異常長のHID reportを受信してもクラッシュせず、観測可能なカウンタまたはエラーとして扱う。
- 存在しない機器への接続失敗、peer消失、Pairing失敗から回復できる。
- HID入力とNotificationを実用的な遅延で処理し、Write Without Responseの連続送信を妨げない。
- 未使用プロファイルのコード・テーブルをリンク結果へ強制的に含めない構成を推奨要件とする。

## 対象外

LE Audio、Bluetooth Mesh、Matter provisioning、OTA/DFU方式の統一、Apple/Google固有サービスの標準搭載、
医療機器としての適合保証、OS固有問題の完全な吸収、ESP-IDFネイティブ公開API。無印ESP32のClassic
SPP / HID / A2DP / AVRCP / HFPは検証済み範囲だけを対応扱いとし、機能ごとの検証状態を文書へ明記する。
Classic・BLEの同時利用（dual-host）は実験扱いのままとする。
codec、PCM処理、device I/OはEspBleの対象外とする。

## 成功条件

- CentralとPeripheralを同一スケッチで構成でき、GATT ClientとServerを同時利用できる。
- 複数Serviceを合成でき、任意UUIDのServiceをライブラリ改造なしで扱える。
- Connection、エラー、Security状態をアプリケーションから確認できる。
- ESP32-S3 2台のPeerテストで主要な接続状態遷移を再現可能に検証できる。
- HID Keyboard Host/Deviceを`ESP32KeyBridge`のadapterから利用できる。
