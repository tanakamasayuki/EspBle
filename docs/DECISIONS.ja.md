# 設計決定の台帳

## 確定

1. Arduino向け単一ライブラリ`EspBle`として提供する。
2. Arduino-ESP32に同梱されたBLEライブラリのNimBLE backendを使用し、外部NimBLE-Arduinoを必須依存にしない。Bluedroid backendは対象外とする。
3. Bluetooth Classicは扱わない。
4. Central / PeripheralとGATT Client / Serverを同じスタック所有者で扱う。
5. APIを単一接続前提に固定しない。
6. 標準Profileと独自Serviceを同じGATT Serverへ合成可能にする。
7. 初期実機ターゲットと自動Peer環境はESP32-S3 2台とする。
8. pytest-embedded-cli上の親DUTと2台目Peerは、既存環境と同じ`s3_peer_host` / `s3_peer_device` profileで識別する。この名前にBLE roleの意味を持たせない。
9. Peerでは両方のsketchを転送・実行でき、両方のSerialを観測・操作できる。初期構成は親側sketchをCentral、`peer_device/`側sketchをPeripheralに固定し、役割を交換しない。EspBle PeripheralはPeer側の結果を主にassertして検証する。
10. Peerの一方は可能な範囲でArduino-ESP32同梱BLE低レベルAPIを直接使い、EspBle同士だけの自己整合テストにしない。
11. 初期プロファイルはHID KeyboardとBattery Serviceに絞る。
12. `memo.ja.md`は正式文書への移行確認後に削除する。
13. 初期自動Peer環境は常設ESP32-S3 2台とする。3台必要な複数接続またはBLE-to-BLE bridge testは、manual用ESP32-S3を追加Peerとして利用し、Peerディレクトリを増やして拡張する。

## 仮置き

1. 通常イベントはstack callbackからqueueへ移し、`update()` contextで配送する。
2. `begin()`前にGATT Server構成を登録し、開始後の動的Service追加は初期版で禁止する。
3. Characteristic valueはbyte sequenceを基本とし、型変換をcodecへ分離する。
4. Connectionはbackend handleの再利用を検出できるlibrary identityを持つ。
5. 初期の同時接続数は制限してよいが、接続単位APIを維持する。

## 優先順位候補

1. HID Mouse / Consumer Control / composite HID
2. Device Information / NUS
3. BLE MIDI
4. reconnect / resubscribe / discovery cache / multiple connections
5. Sensor profile
6. Extended/Periodic Advertising、PHY、Privacy

候補は採用決定ではありません。ユースケース、実機、Peerテスト方法が揃った機能だけを正式スコープへ移します。

## 対象外

- Bluetooth Classic
- LE Audio
- Mesh
- Matter provisioning
- Apple/Google固有Serviceの標準搭載
- OTA/DFU方式の統一
- ESP-IDF native API

## 要確認

- Arduino-ESP32の最小対応版と更新ポリシー
- ESP32-S3以外の初期build matrix
- HID Centralで初期対応するReport Mapの範囲
- Passkey/Numeric Comparisonを初期Securityへ含めるか
- public object ownershipとevent queueの具体API
