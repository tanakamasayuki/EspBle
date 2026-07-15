# 開発計画

## 0. 仕様と基盤

- 要件、コア設計、API規則、決定台帳を作成する。
- Arduino library最小構成とrelease toolkitを導入する。
- pytest-embedded基盤と既存環境互換のESP32-S3 Peer profileを作成する。
- `memo.ja.md`の内容を正式文書へ移行して削除する。

## 1. GAP vertical slice

- ✅ stack lifecycleの最小実装
- ✅ Legacy Advertising builderの最小実装
- ✅ Scannerと値型Scan Result
- ✅ Central接続とPeripheral接続受け入れ
- ✅ Connection identityと切断
- ✅ `advertise_scan` Peerテスト
- ✅ `connect_disconnect` Peerテスト

この段階で所有権、event context、Result型を実コードで確定します。

## 2. 汎用GATT

- ✅ GATT Server Service/Characteristic
- ✅ 既知UUIDを指定するGATT Client Discovery
- ✅ Read / Write with Response
- Service/Characteristic一覧Discovery
- Write Without ResponseのPeer検証
- ✅ Notify / Indicate / subscription / unsubscribe
- MTU
- ✅ `gatt_read_write` Peerテスト
- ✅ `notify_indicate` Peerテスト

## 3. Security

- PairingとBonding
- encrypted Characteristic
- Bond一覧と削除
- `security_bond` / `bond_reconnect` Peerテスト

## 4. 初期Profile

- Battery Service Server/Client
- HID Keyboard Peripheral
- HID Keyboard Central
- HID Output Report
- `battery` / `hid_keyboard` / `hid_keyboard_output` Peerテスト
- ESP32KeyBridge adapterの接続点検証

## 5. 初期リリース判定

- examples compile matrix
- Peer test反復
- Android/Linuxなど複数実装とのmanual interoperability
- memory/error/reconnect試験
- README/API/Release Checklist完成

## 以後

`DECISIONS.ja.md`の候補から1機能ずつ採用し、要件、設計、example、unit/build/Peer/manual testを同時に追加します。
