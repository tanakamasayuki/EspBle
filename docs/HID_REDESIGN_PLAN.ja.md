# HID再設計 作業リスト（複合HID + EspUsbHost/Device流のAPI）

## 目的

現在のEspBleのHIDは`EspBleHidKeyboardDevice` / `EspBleHidKeyboardHost`のkeyboard専用で、1つのHID Serviceにkeyboard Reportしか置けず（複合不可）、mouse/consumer等の入口がありません。これを次の2点で作り直します。

1. **複合HID対応**: 1つのHID Serviceにkeyboard/mouse/consumer/system/gamepadのReportをReport IDで同居させ、同時利用できるようにする。
2. **API統一**: EspUsbDevice / EspUsbHostのHID APIに形を寄せ、3ライブラリで使い勝手を揃える。

初回リリース前の0.x（互換性保証なし、[DECISIONS](DECISIONS.ja.md)確定#22）につき、**破壊的変更を許容**します。

**この再設計は初回リリース(0.1.0)のスコープに含めることを決定済み（2026-07-18）。** 現状のkeyboard専用APIのままでは初回リリースを出さず、本計画の完了を初回リリースの要件とします。HID以外のラップ改善（[FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)のA/B/C）は0.2.0以降です。

## 寄せる相手のAPI形（調査済み）

- **EspUsbDevice**: コア`EspUsbDevice`＋HID種別ごとの別オブジェクト（`EspUsbDeviceHidKeyboard/Mouse/ConsumerControl/SystemControl/Gamepad/Custom`）。各オブジェクトがdeviceへ自己登録し、1回の`begin()`で固定Report ID（keyboard=1,mouse=2,gamepad=3,consumer=4,system=5,vendor=6）のcomposite HIDに合成。keyboardは生usage経路（`sendReport`/`pressUsage`）とlayout-aware経路（`write`/`pressKey`/`setLayout`）の二本立て。mouseは`move/click/press/release/wheel`、consumer/systemは`press/release/click`。keyboardのLEDは`onOutputReport`。
- **EspUsbHost**: 単一`EspUsbHost`が全デバイスを多重化。`onKeyboard/onKeyboardState/onMouse/onConsumerControl/onSystemControl/onGamepad`等のcallback。各イベントは共通base（device identity＋raw bytes）＋`address`＋`interfaceNumber`。出力/クエリは末尾に`address=ANY`。layoutはhost単位、ascii/unicodeをイベントで配送。

BLEでの対応: USBの`address`＝BLEの`connectionId`、USBの「1 deviceに複数HID interface」＝BLEの「1 HID Serviceに複数Report ID（composite）」。

## Phase 0: 設計確定（確定済み 2026-07-18）

- [x] **Device側の入口方式**: EspBleのアクセサ流儀に合わせ`ble.hidKeyboard()` / `ble.hidMouse()` / `ble.hidConsumerControl()` / `ble.hidSystemControl()` / `ble.hidGamepad()`。`configure()`した種別だけがcomposite HID Serviceに登録される（`scanner()`/`gattServer()`と同じアクセサ方式）。
- [x] **Host側の入口方式**: 単一入口`ble.hidHost()`（現`hidKeyboardHost()`を改名）に`onKeyboard/onKeyboardState/onMouse/onConsumerControl/onSystemControl/onGamepad`を集約。
- [x] **Report ID割当**: USBと同じ固定（keyboard=1, mouse=2, gamepad=3, consumer=4, system=5）。
- [x] **命名**: Device型は`EspBleHidKeyboard`/`EspBleHidMouse`…（"Device"を落とす）、Hostイベント型は`EspBleHidKeyboardEvent`/`EspBleHidMouseEvent`…。DeviceとHostの区別は入口で表す。
- [x] **共通イベントbase**: Host側イベントに`connectionId`＋`reportId`＋raw bytesの共通base（`EspBleHidReport`）を設ける。
- [x] **KeyBridge連携**: `EspBleHidKeyboardState`は維持。`hidHost()`改名にadapterを追従。

### 共通API方針（確定）

**初期化は種別ごとに差異を許容し、初期化・接続後の運用APIはなるべく共通の骨格に揃える。** 各種別は同じ骨格メソッドを持ち、種別固有の便利メソッドをその上に上乗せする。

**Device側（送信）**

すべてのHID Device種別が共通で持つ骨格:

| 共通メソッド | 意味 |
|---|---|
| `configure(<種別ごとのConfig>)` | 初期化。**種別ごとに差異あり**（keyboard=layout/reportId/PnP、mouse=ボタン数など） |
| `configured() const` | 構成済みか |
| `sendReport(<種別のReport型>)` | 明示送信（生に近い共通入口） |
| `releaseAll()` | 押下・移動状態の全解除 |

戻り値は全種別で`bool`＋`ble.lastError*()`に統一。種別固有の便利メソッドを上乗せ:

- keyboard: `write()` / `pressKey()` / `tapKey()` / `pressUsage()` / `setLayout()` / `layout()` / `onOutputReport()`
- mouse: `move()` / `click()` / `press()` / `release()` / `wheel()`
- consumer: `press(usage)` / `release()` / `click(usage)`
- system: `press(usage)` / `release()` / `click(usage)`
- gamepad: `send(...)`

**Host側（受信）**

すべての種別で共通:

| 共通 | 内容 |
|---|---|
| `ble.hidHost().discover(connectionId)` | 接続後のHID Discovery（種別横断で全対応Reportを購読） |
| `ready(connectionId) const` | Discovery完了済みか |
| `on<種別>(callback)` / `add<種別>Listener()` / `removeListener()` | 同じ登録形 |
| 全イベントが`EspBleHidReport`（`connectionId`/`reportId`/生Report）を継承 | 種別横断で同じ土台 |

keyboard固有の`setKeyboardLeds()` / `setKeyboardLayout()` / `keyboardLayout()`のみ種別固有として残す。

この方針により、利用者は「種別ごとに`configure()`で初期化 → あとは共通の骨格（`sendReport`/`releaseAll`、`on<種別>`）＋必要なら便利メソッド」という一貫した流れで扱える。

## Phase 1: Device backend一元化（土台・最重要）

現状は`EspBleHidKeyboardDeviceImpl`がHID/DIS/Battery Serviceを自前登録し`server->start()`まで呼んでいる（[EspBle.cpp:2653-2814](../src/EspBle.cpp#L2653)）。これを種別横断のマネージャへ集約する。

- [ ] HID Device共通マネージャ（内部）を新設し、HID Service（0x1812）・DIS・Battery Serviceの登録と`server->start()`を**一元化**する（各種別profileは自分のReport定義だけを提供）。
- [ ] 有効化された種別のReport Descriptorを結合して**composite Report Mapを生成**（各種別に固定Report IDを付与）。
- [ ] Report IDごとのInput Report characteristic（0x2A4D＋Report Reference）を動的に構築し、notifyを種別＋connectionでルーティング。
- [ ] CCCD購読gate・暗号化permission（HOGP Security Mode 1 Level 2）・切断時の全releaseを**種別横断**で適用（既存keyboard実装のロジックを一般化）。
- [ ] Peerテスト（赤→緑）: composite（keyboard+mouse）のReport Map／複数Report Reference／各Report notifyを検証。

## Phase 2: Device API（EspUsbDevice流）

- [ ] `EspBleHidKeyboard`: 生usage経路（`sendReport`/`pressUsage`/`releaseAll`）＋layout-aware経路（`write`/`pressKey`/`tapKey`/`setLayout`/`layout`）＋`onOutputReport`（LED、decoded bool）。ascii→usage逆変換はkeymap共有（下記Phase 2末）。
- [ ] `EspBleHidMouse`: `move(x,y,wheel,buttons)`/`click`/`press`/`release`/`wheel`/`releaseAll`/`sendReport`。ボタンbit定数。
- [ ] `EspBleHidConsumerControl`: `press(uint16 usage)`/`release`/`click`/`sendUsage`。メディアキーusage定数。
- [ ] `EspBleHidSystemControl`: `press(uint8 usage)`/`release`/`click`。電源系usage定数。
- [ ] `EspBleHidGamepad`: `send(...)`/`sendReport(EspBleHidGamepadReport)`/`releaseAll`。hat/button定数。
- [ ] Device側keymap: ascii→usage逆変換（EspUsbDevice同様、`EspBleKeymap.h`のtableを逆引き）。keyboardの`write()`/`pressKey()`用。

## Phase 3: Host backend一般化

- [ ] `EspBleHidReportMap.h`のparserを、keyboard専用判定から**複数種別の識別**へ拡張（mouse=Generic Desktop/Mouse、consumer=Consumer Page、system=Generic Desktop/System Control、gamepad=Generic Desktop/Gamepad|Joystick）。各種別のReport IDと入力フォーマットを返す。
- [ ] `discover()`で検出した全対応Reportを購読し、Report IDごとに種別を紐付け。
- [ ] 受信Input ReportをReport ID→種別で振り分け、種別別イベントへparse（mouseの相対値、consumer/systemのusage、gamepadのfield分解）。
- [ ] unitテスト（赤→緑）: 各種別のReport Map descriptorとInput Report parseを`tests/unit/report_map`へ追加。

## Phase 4: Host API（EspUsbHost流）

- [ ] `ble.hidHost()`（`hidKeyboardHost()`から改名）に`onKeyboard`/`onKeyboardState`/`onMouse`/`onConsumerControl`/`onSystemControl`/`onGamepad`を集約。既存のlistener API（`add*Listener`/`removeListener`）も種別ごとに提供。**実装はEspUsbHostが先行実装した方式に倣う**（下記付録参照）: listener callbackを`std::shared_ptr`で保持し、配送時はshared ownershipをsnapshotする（`std::function`のコピーを避け、mutable callback状態を保持し、イベントごとの動的コピーもしない）。IDはHostインスタンス内でイベント種別をまたいで一意。registryはmutexで保護するがcallback実行中はロックしない。単一`on*`→listener登録順で配送し、callback内の追加・解除は次イベントから反映する。現行EspBleは配送のたびに`std::function`を配列コピーしている（`EspBle.cpp`の`dispatchPendingEvents`）ため、この点を改善する。
- [ ] イベント型: `EspBleHidKeyboardEvent`（ascii/unicode/modifiers、既存踏襲）、`EspBleHidMouseEvent`（x/y/wheel/buttons/moved/buttonsChanged）、`EspBleHidConsumerControlEvent`（usage/pressed/released）、`EspBleHidSystemControlEvent`、`EspBleHidGamepadEvent`。いずれも`connectionId`＋`reportId`を持つ共通baseを継承。
- [ ] `setKeyboardLeds`/`setKeyboardLayout`/`keyboardLayout`/`ready`は維持。
- [ ] Peerテスト（赤→緑）: composite peer deviceに対しHostが各種別を受信できることを検証。

## Phase 5: 既存テスト・回帰の追従

- [ ] `hid_keyboard_device`/`hid_keyboard_host`/`hid_robustness`/`hid_boot_keyboard`/`ble_keybridge_keyboard`をAPI変更へ追従改修。
- [ ] KeyBridge input adapter試作（`EspBleHidKeyboardState`利用）を`hidHost()`改名へ追従。
- [ ] 全Peer＋unit回帰。

## Phase 6: example・ドキュメント

- [ ] example追加: `Hid/Mouse`（Device）、`Hid/ConsumerControl`（メディアキー）、`Hid/CompositeKeyboardMouse`（複合）、Host側の受信example。既存`Hid/KeyboardDevice`/`KeyboardHost`をAPI変更へ追従。各日英READMEペア。
- [ ] docs再編: `HID_KEYBOARD_DEVICE_SPEC`/`HID_KEYBOARD_HOST_SPEC`を種別横断の`HID_DEVICE_SPEC`/`HID_HOST_SPEC`へ再構成。`API_DESIGN`のHID節を書き換え。`DECISIONS`へ複合HID・命名・Report ID割当を記録。`FEATURE_MATRIX`のmouse/consumer/system/gamepadを✅へ、複合HIDを✅へ更新。`STATUS`/`CHANGELOG`/`keywords.txt`/`README`更新。

## 進め方の原則

- 各Phaseは**テストファースト**（Peer/unitで赤を作ってから実装）。
- Phase 1（backend一元化）が全ての土台。ここが済めばDevice各種別（Phase 2）は追加が軽い。
- Host（Phase 3-4）はDeviceと独立に進められるが、composite peer deviceがあるとPeerテストが書きやすいのでDevice先行が効率的。
- 破壊的変更のため、旧API名は残さず一括で切り替える（0.xの方針）。

## 規模感（目安）

| Phase | 規模 | 依存 |
|---|---|---|
| 0 設計確定 | 小（決定のみ） | — |
| 1 Device backend一元化 | 大 | Phase 0 |
| 2 Device各種別API | 中（1で土台ができれば種別ごとは小） | Phase 1 |
| 3 Host parser一般化 | 中〜大 | Phase 0 |
| 4 Host API | 中 | Phase 3 |
| 5 既存テスト追従 | 中 | Phase 2,4 |
| 6 example/docs | 中 | Phase 2,4,5 |

## 付録: 3ライブラリ横断の整合 / 相互フィードバック候補

再設計にあたり、EspBle / EspUsbHost / EspUsbDeviceで揃えると相互運用・学習コストが下がる点、およびEspBleの設計から両USBライブラリへ逆提案できる候補。実施前に各プロジェクトへ修正依頼文で（過去のkeymap依頼と同様に）すり合わせる。

**3ライブラリで揃える**

- HID usage定数・modifier bit・mouse button bit・consumer/system usage定数の**名前の語幹と値を統一**（prefixはライブラリ固有で可: `ESP_USB_HID_KEY_*` / `ESP_USB_HOST_*` / `ESP_BLE_HID_*`）。値はHID標準なので本来一致するはずで、語幹を揃えて移植コストを下げる。
- Report ID割当（keyboard=1, mouse=2, gamepad=3, consumer=4, system=5）。
- layout enum（Windows LCID）とkeymap tableは既に共有済み。維持する。

**EspBle → USB側へのフィードバック候補（要確認）**

- **listener方式（複数購読 `add*Listener`/`removeListener`）**: EspBleはadapterとsketchのcallback共存のため実装済み。EspUsbHostが単一callbackのみなら、同じ用途（ESP32KeyBridge adapter＋sketchの併用）で有用なはずで、追加を提案できる。
- EspBle側で再設計中に見つかった良い抽象があれば随時ここへ追記し、フィードバック対象とする。

**USB側 → EspBleが取り込む候補**

- **gamepadのfield分解**（`EspUsbHostHIDFieldValue`: usagePage/usage/logicalMin/Max/bitOffset/bitSize）: gamepadは形式が多様なので、EspBleのHost gamepadイベントもfield分解方式に倣う（Phase 4のgamepadイベント設計へ反映）。
- **共通イベントbase**（device identity＋raw report bytes）: EspBleのHostイベントにも共通baseを設ける（Phase 0で反映済み）。
- **custom / vendor HID経路**: 任意Report Descriptorの送受信入口（EspUsbDeviceHidCustom / EspUsbHost onHIDInput相当）をEspBleにも設けるか将来検討（初期は見送り可）。
- **listener registryの実装方式（EspUsbHostで実装済み、逆輸入）**: EspUsbHostがlistener提案を消化し、6種のパース済みHID入力へ固定長listener registryを実装（実機peer test 56件PASS）。その際、EspBleの現行実装より洗練された方式を採用したので、EspBleはHID再設計のPhase 4でこれに揃える。具体的には:
  1. 配送時に`std::function`をコピーせず**shared ownershipをsnapshot**し、解除との競合を防ぎつつmutable callback状態を保持、イベントごとの動的コピーも回避する。
  2. IDはインスタンス内でイベント種別をまたいで一意。
  3. registryはmutexで保護するがcallback実行中はロックしない。
  4. 単一`on*` callback → listener登録順で配送する。
  5. callback内の追加・解除は次イベントから反映する。
  （現行EspBleは`dispatchPendingEvents`で配送のたびに`std::function`を固定長配列へコピーしており、上記1の点で劣る。）

いずれのフィードバックも、各プロジェクトの事情があるため一方的に変更せず、依頼文でのすり合わせを前提とする。
