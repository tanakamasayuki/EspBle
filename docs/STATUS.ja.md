# 開発状況とTODO

最終更新: 2026-07-16

この文書は、EspBleの現在地と次に着手する作業を短く確認するための進捗メモです。確定仕様は各仕様書と`DECISIONS.ja.md`を正とし、この文書では実装状況、優先順位、未解決事項を追跡します。

## 現在地

初期ターゲットであるESP32-S3、Arduino-ESP32 3.3.10、同梱NimBLE backendで、BLEの共通基盤とHID Keyboard Host / Deviceの最初のvertical sliceが動作しています。ESP32-S3 2台のPeerテストで実radioを使用した自動検証が可能です。

ESP32KeyBridge用途についても、BLE Keyboard入力をraw HID usageのままKeyBridgeへ渡し、remap、modifier、切断release、LED返送、Bond再接続後の再Discoveryまで検証済みです。ただし公開APIはまだ確定しておらず、初回リリース前の設計整理と耐久・相互運用試験が残っています。

## 実装・検証済み

### BLE共通基盤

- ✅ Arduino-ESP32同梱NimBLEの初期化と終了
- ✅ Legacy Advertising、Scanning、Scan Resultの値型copy
- ✅ Central接続、Peripheral接続受け入れ、切断、Connection ID
- ✅ GATT Server / Clientの既知UUID Discovery、Read、Write with Response
- ✅ Notify / Indicate、購読、解除、CCCD event
- ✅ 接続時MTU交換、Connection snapshot、payload上限検証
- ✅ callbackを`ble.update()` contextへ配送
- ✅ 基本的なエラー名と詳細

### Security

- ✅ Just Works、LE Secure Connections、Bonding
- ✅ encrypted / authenticated Characteristic permission
- ✅ Bond一覧、個別削除、全削除、Bond再接続
- ✅ DisplayOnly / KeyboardOnlyの静的6桁passkeyとMITM

### HID Keyboard Device

- ✅ Report Protocolの固定6KRO Keyboard
- ✅ Input Report、全release、Output LED Report
- ✅ HID / Device Information / Battery Serviceの合成
- ✅ Report ID、Report Map、Report Reference
- ✅ Battery Level更新

### HID Keyboard Host

- ✅ HID Service DiscoveryとInput Report購読
- ✅ Report Map / Report Referenceから固定6KROを識別
- ✅ 256-bit Keyboard Usage snapshotとchanged bitmap
- ✅ modifier単独eventと切断時の全release
- ✅ `onKeyboard()` press / release convenience event
- ✅ EspUsbHost互換の19 layout識別子・usage-to-8-bit変換table
- ✅ HID Information country code、Battery Level read、LED Output write
- ✅ 単一`on*()`と共存する固定容量listener登録・解除

### ESP32KeyBridge接続

- ✅ raw usage snapshotからKeyBridge `KeySet`への写像
- ✅ KeyBridgeによるremapとmodifier同時押し
- ✅ BLE切断時のstuck key防止
- ✅ KeyBridge LockStateからBLE Keyboard LEDへの返送
- ✅ Bond再接続、新Connection IDでの再Discovery、LED状態復元
- ✅ sketch callbackとKeyBridge adapter listenerの共存

### 自動Peerテスト

以下のscenarioをESP32-S3 2台で実装済みです。

- ✅ `stack_smoke`
- ✅ `advertise_scan`
- ✅ `connect_disconnect`
- ✅ `gatt_read_write`
- ✅ `notify_indicate`
- ✅ `mtu`
- ✅ `security_bond`
- ✅ `security_passkey`
- ✅ `hid_keyboard_device`
- ✅ `hid_keyboard_host`
- ✅ `ble_keybridge_keyboard`

## 現在の主な制限

- 公開APIは試行段階で、互換性を保証する初回リリース前です。
- HID Keyboardは固定6KROのみです。Boot Protocol、NKRO、Consumer Control、Mouse、複合HIDは未対応です。
- HID Hostの再接続では、利用者がscan/connectし、Security完了後に新しいConnection IDで`discover()`を再実行します。
- GATT Client Discoveryは既知UUID指定のみで、Service / Characteristic一覧列挙は未実装です。
- Central側GATT operationは同時1件です。operation queue、ID、cancelは未実装です。
- 実行時passkey入力、Numeric Comparison、Pairing確認・拒否UIは未対応です。
- callback event queueの容量・overflow APIと、GATT payloadの最終的なbyte container型は未確定です。
- 自動実機検証はESP32-S3中心です。ESP32-P4 + C6 Hosted BLEなどは対応候補ですが未検証です。
- 市販BLE KeyboardやAndroid / Linux / Windows / macOSとのmanual interoperabilityは未完了です。

## 次の作業

### P0: 初回リリースまでに優先する作業

1. **ESP32KeyBridge利用形から公開APIを固める**
   - `ble.update()`を必須の最終仕様にするか決める。
   - HID Discoveryを明示呼出しのままにするか、自動化optionを設けるか決める。
   - `setKeyboardLeds()`を同期`bool` helperとして維持するか、非同期Resultへ揃えるか決める。
   - EspBle API確定後、正式なBLE input adapterをESP32KeyBridge側へ移す。

2. **共通APIの未確定事項を整理する**
   - Result型とasync operation ID。
   - GATT payloadのbyte containerと最大長。
   - event queue容量、overflow取得、listener APIを他領域へ広げる範囲。
   - object / handle ownershipと複数接続時の送信対象。
   - 採用事項を`DECISIONS.ja.md`へ移す。

3. **障害・再接続・耐久試験を追加する**
   - peer loss、接続timeout、Notification中の切断。
   - 再接続と再購読の反復。
   - queue overflow、異常payload、GATT operation競合。
   - 長時間実行時のheap、task、Bond/NVS状態。
   - Peer suite全体の連続実行とflaky要因の除去。

4. **初回リリース用のテストと文書を完成させる**
   - 全exampleのcompile matrixを自動化する。
   - host上のunit testを追加する（Advertising parser、codec、layout、状態遷移、error変換）。
   - 市販BLE KeyboardとAndroid / Linuxなどでmanual interoperabilityを確認する。
   - README、API説明、CHANGELOG、Release Checklistを更新する。
   - `memo.ja.md`の内容が正式文書へ移行済みか確認し、完了後に削除する。

### P1: 初期基盤を補完する作業

- Write Without ResponseのPeer検証。
- Service / Characteristic一覧Discovery。
- Battery Service standalone Client / Server。
- 実行時passkey入力とNumeric Comparison。
- ESP32-S3以外のcompile matrixと実機検証。
- 3台Peerによる複数接続、またはBLE-to-BLE bridge E2E。

### P2: 初回リリース後に優先順位を決める候補

1. HID Consumer Control
2. HID Mouse
3. NKRO / composite HID
4. Device Information / NUS
5. BLE MIDI
6. reconnect cache、複数接続強化
7. Sensor profile
8. Extended / Periodic Advertising、PHY、Privacy

これらは採用決定ではありません。利用例、実装量、Peerテスト方法を確認し、1機能ずつ正式スコープへ移します。

## 直近の推奨順序

1. KeyBridge adapter試作を基準に`update()`、Discovery、LED writeの仕様を決める。
2. 決まったAPIでKeyboardHost / KeyboardDevice exampleを整理する。
3. peer lossと再接続反復テストを追加する。
4. examples compile matrixとhost unit testを整備する。
5. manual interoperabilityを実施し、初回リリース判定へ進む。

## 初回リリース判定の目安

- [ ] 公開するAPIと初期対応範囲が文書上で確定している。
- [ ] 全exampleが対象build matrixでコンパイルする。
- [ ] 全Peerテストを反復実行して安定して通過する。
- [ ] 切断、再接続、peer lossでstuck keyや資源リークがない。
- [ ] 市販機器と少なくとも2種類の外部BLE実装で相互運用できる。
- [ ] README、API文書、CHANGELOG、Release Checklistが揃っている。
- [ ] `memo.ja.md`の移行確認と削除が完了している。

## 関連文書

- [要件](REQUIREMENTS.ja.md)
- [コア設計](CORE_DESIGN.ja.md)
- [API設計](API_DESIGN.ja.md)
- [HID Keyboard Device仕様](HID_KEYBOARD_DEVICE_SPEC.ja.md)
- [HID Keyboard Host仕様](HID_KEYBOARD_HOST_SPEC.ja.md)
- [設計決定](DECISIONS.ja.md)
- [開発計画](DEVELOPMENT_PLAN.ja.md)
- [テスト計画](../tests/TEST_PLAN.ja.md)

## 更新ルール

- 機能を実装しただけでは完了にせず、対応するbuild / Peer / manual testの状況も記録する。
- 仕様を確定した項目は`DECISIONS.ja.md`へ移し、この文書では完了または次の課題だけを残す。
- 優先順位が変わった場合はP0 / P1 / P2と「直近の推奨順序」を同時に更新する。
- 日付と実装状況はまとまった作業単位ごとに更新する。
