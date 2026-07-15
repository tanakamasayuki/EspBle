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
- ✅ MTU設定、Connection snapshot、payload上限検証
- ✅ `gatt_read_write` Peerテスト
- ✅ `notify_indicate` Peerテスト
- ✅ `mtu` Peerテスト

## 3. Security

- ✅ Just Works PairingとBonding
- ✅ encrypted Characteristic
- ✅ Bond一覧と削除
- ✅ `security_bond` Peerテスト（Bond再接続を含む）
- ✅ 静的Passkey / MITMと`security_passkey` Peerテスト
- 実行時Passkey入力 / Numeric Comparisonの設計とPeerテスト

## 4. 初期Profile

- ✅ Battery Service Server（HID Keyboard Deviceへの組込み）
- Battery Service Client / standalone Server
- ✅ HID Keyboard Device（Peripheral / GATT Server）
- ✅ HID Keyboard Host（Central / GATT Client、初期6KRO）
- ✅ HID Keyboard Hostの`onKeyboard()` press/release convenience event
- ✅ layout選択と任意のusage-to-ASCII変換
- ✅ EspUsbHost互換の19 layout識別子・変換table
- ✅ HID Information country codeの取得（layout自動決定には使用しない）
- ✅ HID Output Report
- ✅ `hid_keyboard_device` Peerテスト（Battery ReadとOutput Reportを含む）
- ✅ `hid_keyboard_host` Peerテスト
- `battery` standalone / NKRO・複合HID Host Peerテスト
- ✅ ESP32KeyBridge input adapterの接続点検証（試作adapterとPeerテスト）
- ESP32KeyBridge側への正式adapter追加（EspBle公開API確定後）

## 5. 初期リリース判定

- examples compile matrix
- Peer test反復
- Android/Linuxなど複数実装とのmanual interoperability
- memory/error/reconnect試験
- README/API/Release Checklist完成

## 以後

`DECISIONS.ja.md`の候補から1機能ずつ採用し、要件、設計、example、unit/build/Peer/manual testを同時に追加します。
