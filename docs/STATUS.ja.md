# 開発状況とTODO

最終更新: 2026-07-18

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
- ✅ `lifecycle_stress`
- ✅ `hid_robustness`
- ✅ `hid_security`
- ✅ `hid_boot_keyboard`
- ✅ `advertise_payload`

host上のunit test（`tests/unit/`）としてkeymap変換とHID Report Map parserを実装済みです。

## 現在の主な制限

- 公開APIは試行段階で、互換性を保証する初回リリース前です。
- HID Keyboardは固定6KROのみです。Boot Protocol、NKRO、Consumer Control、Mouse、複合HIDは未対応です（Report Map parserはConsumer Control等を併載するkeyboardからkeyboard Input Reportを選択できますが、解釈するのはkeyboard reportのみです）。
- HID Hostの再接続では、利用者がscan/connectし、Security完了後に新しいConnection IDで`discover()`を再実行します。
- GATT Client Discoveryは既知UUID指定のみで、Service / Characteristic一覧列挙は未実装です。
- Central側GATT operationは同時1件です。operation queue、ID、cancelは未実装です。
- Central接続はScan Result指定のみで、address直接指定の接続は未実装です。
- 切断理由の取得と接続パラメータ更新のAPIは未実装です。
- GATT ServerのDescriptor定義とRead時callbackは未実装です（値保持と`onWritten()`のみ）。
- GATT ClientのDescriptor Read / Writeと操作単位のtimeout指定は未実装です。
- 実行時passkey入力、Numeric Comparison、Pairing確認・拒否UIは未対応です。
- passkey表示イベントのConnectionは「最初の未暗号化Connection」の推定です。複数接続の同時Pairingでは誤ったConnectionを報告する可能性があります（DECISIONS Security #8）。
- Central側のMTUは接続時のsnapshotのみで、接続後の変化は追跡できません（同梱backendにMTU変更callbackがないため）。MTU交換が接続timeoutまでに完了しない場合、snapshotが23になる可能性があります（DECISIONS Connection/GATT #23）。
- callback event queueは満杯時にNotification / key stateのみをdropし（lifecycle・完了イベントは優先保持）、drop数を`droppedEventCount()`等で観測できます。queue容量はcompile-time定数で、実行時設定APIとoverflowイベントは設けません（DECISIONS 確定 #21）。
- 自動実機検証はESP32-S3中心です。ESP32-P4 + C6 Hosted BLEなどは対応候補ですが未検証です。
- Bluedroidが既定のSoC（無印ESP32など、NimBLEを同梱しない構成）は対象外です。将来、別ライブラリ（`EspBleBluedroid`等）で「似た使い方・完全互換ではない」対応を検討します（DECISIONS 確定 #23）。
- 市販BLE KeyboardやAndroid / Linux / Windows / macOSとのmanual interoperabilityは未完了です。

## 次の作業

### P0: 初回リリースまでに優先する作業

> **初回リリース(0.1.0)のスコープ決定（2026-07-18）**: HIDは現状のkeyboard専用APIのままでは出さず、**複合HID対応＋EspUsbDevice/Host流のAPI再設計**（[HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md)）を初回リリースに含める。破壊的変更を伴うが0.x（互換性保証なし）のうちに実施する。HID以外のラップ改善（[FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)のA/B/C）は0.2.0以降。

0. **HID複合対応・API再設計を完了する**（初回リリースの主目玉、[HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md) Phase 1-6）。

1. **ESP32KeyBridge利用形から公開APIを固める**
   - ✅ `ble.update()`必須を最終仕様として確定（DECISIONS 確定 #17）。
   - ✅ HID Discoveryは明示`discover()`維持、security有効時はSecurity完了後に呼ぶ規範で統一（DECISIONS HID Host #17）。
   - ✅ `setKeyboardLeds()`はWrite Without Response優先の同期`bool` helperとして確定（DECISIONS HID Host #6）。
   - 正式なBLE input adapterは初回リリースの要件から外し、リリース後にそのリリース版へ対応するadapterをESP32KeyBridge側で作成してもらう（2026-07-18裁定）。

2. **共通APIの未確定事項を整理する**
   - ✅ Result型の役割分担とoperation ID見送りを確定（DECISIONS 確定 #19）。
   - ✅ byte containerはpointer+length+`String` overloadで確定、値型containerへの移行余地のみ残す（DECISIONS 確定 #20）。
   - ✅ event queue容量・overflow方針を確定（DECISIONS 確定 #21）、listener APIの一般化は見送り（DECISIONS HID Host #10）。
   - object / handle ownershipと複数接続時の送信対象は複数接続実装時に決める。

3. **障害・再接続・耐久試験を追加する**
   - ✅ peer loss（supervision timeout）、接続timeoutの非同期失敗、Notification中の切断（`lifecycle_stress`）。
   - ✅ 再接続と再購読の反復（`lifecycle_stress`のheap検証付き反復）。
   - ✅ queue overflow、異常payload、GATT operation競合（`lifecycle_stress` / `hid_robustness` / `hid_boot_keyboard`）。
   - 長時間実行時のheap、task、Bond/NVS状態。
   - Peer suite全体の連続実行とflaky要因の除去。

4. **初回リリース用のテストと文書を完成させる**
   - ✅ 全exampleのcompile matrixを自動化する（`.github/workflows/compile-examples.yml`、esp32s3 profile、push/PRで実行）。
   - ✅ host上のunit testを追加する（keymap変換とReport Map parserを`tests/unit/`で実装済み。Advertising parser等の追加は任意）。
   - 市販BLE KeyboardとAndroid / Linuxなどでmanual interoperabilityをある程度確認する（必須の合格基準にはしない。2026-07-18裁定、ユーザーが実施）。
   - README、API説明、CHANGELOG、Release Checklistを更新する。
   - examplesを充実させ、触りながら公開APIの確定判断を行う（診断系`Info/`を追加済み。追加候補があれば継続）。

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
9. Beacon / Connectionless（iBeacon、Eddystone、任意Advertisingデータ）

これらは採用決定ではありません。利用例、実装量、Peerテスト方法を確認し、1機能ずつ正式スコープへ移します。

## 直近の推奨順序

1. ✅ KeyBridge adapter試作を基準に`update()`、Discovery、LED writeの仕様を決める。
2. ✅ 決まったAPIでKeyboardHost / KeyboardDevice exampleを整理する。
3. ✅ peer lossと再接続反復テストを追加する。
4. ✅ examples compile matrixとhost unit testを整備する。
5. manual interoperabilityを実施し、初回リリース判定へ進む。

## 初回リリース判定の目安

- [ ] 公開するAPIと初期対応範囲が文書上で確定している。
- [ ] 全exampleが対象build matrixでコンパイルする。
- [ ] 全Peerテストを反復実行して安定して通過する。
- [ ] 切断、再接続、peer lossでstuck keyや資源リークがない。
- [ ] 市販機器と少なくとも2種類の外部BLE実装で相互運用できる。
- [ ] README、API文書、CHANGELOG、Release Checklistが揃っている。
- [x] `memo.ja.md`の移行確認と削除が完了している（2026-07-18）。

## 関連文書

- [要件](REQUIREMENTS.ja.md)
- [コア設計](CORE_DESIGN.ja.md)
- [API設計](API_DESIGN.ja.md)
- [HID Keyboard Device仕様](HID_KEYBOARD_DEVICE_SPEC.ja.md)
- [HID Keyboard Host仕様](HID_KEYBOARD_HOST_SPEC.ja.md)
- [設計決定](DECISIONS.ja.md)
- [機能対応マトリクス](FEATURE_MATRIX.ja.md)
- [開発計画](DEVELOPMENT_PLAN.ja.md)
- [テスト計画](../tests/TEST_PLAN.ja.md)

## 更新ルール

- 機能を実装しただけでは完了にせず、対応するbuild / Peer / manual testの状況も記録する。
- 仕様を確定した項目は`DECISIONS.ja.md`へ移し、この文書では完了または次の課題だけを残す。
- 優先順位が変わった場合はP0 / P1 / P2と「直近の推奨順序」を同時に更新する。
- 日付と実装状況はまとまった作業単位ごとに更新する。
