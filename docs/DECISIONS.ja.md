# 設計決定の台帳

確定した設計判断と、その理由を話題別に記録する。**現在成立しているものだけを載せる**——上書きされた判断や、撤去した同梱`BLE`ラッパの挙動に関する記録は残さない。実装状況は[STATUS.ja.md](STATUS.ja.md)、対応機能は[FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)、テストの中身は[../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md)が正本。

## スコープ

1. Arduino向け単一ライブラリ`EspBle`として提供する。Arduino-ESP32に同梱されたNimBLEを使い、外部NimBLE-Arduinoを必須依存にしない。
2. Bluetooth Classicは扱わない。LE Audio、Mesh、Matter provisioning、OTA/DFU方式の統一、ESP-IDF native APIも対象外。
3. 対象可否はBLE内蔵SoCかどうかではなく、**Arduino-ESP32がNimBLEを提供する構成か**で判断する。ESP32-P4 + ESP32-C6などのHosted BLEも候補に含め、専用build/実機試験後に対応済みとする。
4. Bluedroidが既定のSoC（無印ESP32など）は対象外。API仕様が安定した後に別ライブラリ（`EspBleBluedroid`等）で対応する可能性を残すが、ソース互換は努力目標にとどめる——Bluedroid/NimBLEの内部差でbonding・MTU・securityの細部挙動は必ずずれる。backend非依存の高レベルロジック（`EspBleKeymap.h`、`EspBleHidReportMap.h`、イベント値型、KeyBridge境界）は共有候補。未着手のため利用者向け文書では紹介せず、無印ESP32でBLEが必要な場合は実在のNimBLE-Arduinoを案内する。
5. 公開APIはSemantic Versioningに従う。1.0.0より前の0.x系は試行段階で互換性を保証しない。
6. Central / PeripheralとGATT Client / Serverを同じスタック所有者で扱い、APIを単一接続前提に固定しない。標準Profileと独自Serviceは同じGATT Serverへ合成できる。

## アーキテクチャ

1. **同梱の`BLE`ラッパクラスには依存せず、NimBLEホストAPIを直接使う。** `BLEDevice` / `BLEClient` / `BLEServer` / `BLEScan` / `BLEAdvertising` などを一切使わない。

   理由: 「backendの制約」として記録してきたものの大半が、調査の結果**ラッパの制約であってNimBLEの制約ではなかった**。同一Service内の同一UUID Characteristic（`BLEService::addCharacteristic()`が既存を再利用）、同一UUID Serviceが1つに潰れるCentral側（`BLEClient::m_servicesMap`がUUIDキー）、Directed Advertising（`start()`が`direct_addr = NULL`固定）、スキャン側Filter Accept List（`filter_policy`非公開）、advertisingチャネルマップ、`connect()`のtimeout（`esp_ble_gattc_open()`経由でcancelが空振り）、indication確認のcharacteristic単位セマフォ——いずれもホストAPIを直接呼べば解決する。**GATT client discoveryのヒープリーク（約2.6 KB/discovery）も、ラッパのremoteオブジェクトを作らなくなることで消える。**

   個別に回避せず全面的に外したのは、**部分的な自前化が成立しないため**。NimBLEでは`ble_gap_adv_start()`に渡したコールバックがその広告から成立した接続の全GAPイベント（切断・購読・MTU・indication確認）を受け取るので、広告だけ自前にすると`BLEServer`へイベントが届かずGATT Serverが機能しなくなる。転送もできない——`BLEServer::handleGATTServerEvent()`はprivateで、`BLEDevice::setCustomGapHandler()`はNimBLEパスでは一度も呼ばれない死んだAPIである。

   残るのはビルド構成由来の制約だけ: Extended / Periodic Advertisingは`CONFIG_BT_NIMBLE_EXT_ADV`無効でビルドされたプリビルドライブラリに関数自体が無く、**唯一の真の不可能**。

2. **同一UUIDの重複は仕様が認める限り両役割で扱う。** Peripheralは属性テーブルを`ble_gatts_add_svcs()`で自前に組み、対象は`addService()` / `addCharacteristic()`が返すハンドルで指定する。Centralはdiscoveryを自前に走らせ、read / write / 購読 / descriptor操作をすべて属性ハンドルへ直接発行する。UUIDは「型」であって「どれか」ではないため、UUIDだけを対象指定の手段にしない。

   descriptorにもハンドル指定を用意する。HIDのReport Reference（0x2908）は「0x2A4Dのcharacteristicの下の0x2908」で、UUIDの組では言い表せない。例外は**再接続時の購読自動復元**で、peerアドレスとUUIDを手掛かりにするためUUIDが一意なCharacteristicに限られる。

3. **イベント配送は明示`ble.update()`（呼び出したloop task context）を最終仕様とする。** 内部task配送や選択式は採用しない。`update()`を呼ばない限りconnect/discover等の完了通知も配送されない。stack callback内でユーザーcallbackを実行せず、backendのオブジェクトを外へ出さずに値へcopyしてqueueへ積む。

4. **event queueの容量はcompile-time定数とする。** Arduinoのlibrary buildでは利用者が`-D`で上書きする実用的な手段がないため、容量設定APIは設けない。overflowは専用イベントではなく、dropカウンタ（`droppedEventCount()`等）と**lifecycle・完了イベントの優先保持**で扱う——満杯時は最古のNotificationを追い出し、drop対象をNotificationに限定する。

5. **操作APIの役割分担**: 受理時の同期エラーは`bool`戻り値＋`lastErrorName()` / `lastErrorDetail()`、完了・失敗は各イベントのerror/detailフィールドで通知する。`lastError*`は単一状態のため、操作呼び出しは単一のloop task contextから行うことを前提とする。operation IDと個別の強制cancelは導入しない。

6. **公開の値containerはpointer+lengthを基本とし、`String`を便宜overloadとして提供する。** 値型container（`EspBleBytes`等）への移行余地は残す。

7. **backend非依存のロジックはheader＋host unit testで持つ。** keymap変換（`EspBleKeymap.h`）、HID Report Map parser（`EspBleHidReportMap.h`）、BLE MIDI codec（`EspBleMidi.h`）、IEEE-11073 medical float（`EspBleMedicalFloat.h`）、CGMのE2E-CRC、iBeacon codec（`EspBleIBeacon.h`）。実機を要さず、Bluedroid版とも共有できる。

8. **op毎のタスク生成を常駐workerへ統合する案は見送る（意図的）。** 常駐化はメモリ逼迫時の`xTaskCreate`失敗（全実機テストで未観測）を避ける代わりに、idle時・未使用機能でも常駐task分（HIDは16KB級）を常時確保する。ライブラリとしてはidleコスト0・未使用機能は未確保の現行方式が一般ケースで有利。

9. **Arduino-ESP32 BLE stackが外部で初期化済みの場合は所有権競合として拒否する。** 初期化済みインスタンスへの2回目の`begin()`は、同一configなら成功、異なるconfigなら`InvalidState`で失敗する——黙って旧設定のまま成功を返さない。

10. Descriptor Write eventは`connectionId`を持つ。属性テーブルを自前に組んでいるため、ホストのaccess callbackが`conn_handle`を渡す。

11. 内部worker task（GATT operation、Server送信）は完了イベントをpushしてからbusy flagをクリアする。`end()`はbusy flagのクリアを待ってから共有状態を破棄するため、この順序でuse-after-freeを防ぐ。`end()`は実行中のconnect試行を打ち切り、Scannerの未配送resultをflushして次のsessionへ持ち越さない。

## GAP（Advertising / Scan / Privacy）

1. Legacy Advertisingの31-byte上限で要求fieldが欠落する場合は、黙って落とさず**明示的なargument error**とする。
2. Advertisingのservice UUIDはサイズ（16/32/128-bit）ごとに1つの「Complete List」AD構造へまとめ、同一AD typeをpayload内に複数出現させない（CSS Part A 1.1）。
3. Address privacyは`EspBleConfig::ownAddressType`（`Public`（既定） / `RandomStatic` / `ResolvablePrivate`）で選ぶ。RandomStaticは`BLE_OWN_ADDR_RANDOM`、ResolvablePrivateは`BLE_OWN_ADDR_RPA_RANDOM_DEFAULT`（controllerがRPAを回転生成、`CONFIG_BT_NIMBLE_RPA_TIMEOUT`＝900秒）。RPAはpeerがbonding時のIRKで解決するためsecurity/bonding併用時のみ有用で、回転周期がテスト実時間に合わないため自動テストはrandom staticに留める。
4. iBeaconはmanufacturer specific dataへ載るだけなので、advertising/scan APIを増やさずcodec（`EspBleIBeacon.h`）で完結させる。
5. 汎用のService Data API（`addServiceData()`最大4ブロック、受信側は`serviceDataFor()`）を持つ。**Eddystoneは一度実装したが削除した**——Eddystone/Physical Webは事実上終息したプロトコルで、デッドプロトコルの保守を抱えたくない。iBeaconは現役のため維持し、汎用Service Dataは独立して有用なため残した（判断: 現実の利用価値でスコープを決める）。

## 接続とGATT

1. **GATT Server構成は`begin()`前に全登録し、開始後の動的Service追加は禁止する。** 同梱ビルドは`CONFIG_BT_NIMBLE_DYNAMIC_SERVICE`無効で、開始後に追加したserviceは二度と登録されない。security permissionも登録時に決まるため、この順序は偶然ではなく維持すべき不変条件である。
2. Connectionはbackend handleとは別の**library生成ID**、peer address、local role、MTUを持つ値snapshotとする。handleの再利用を検出できるため、接続と切断で同じIDを通知できる。
3. 待機型の操作（Connect、Discovery、Read、Write、Server送信）は内部taskで実行し、公開操作の受理時にloopをblockしない。
4. **Central側GATT操作はATT上は同時1件だが、呼び出しは自動でFIFOキューへ積まれ順に実行される。** 「operation already in progress」で拒否しないので、利用側は直列化を意識しない。キューは実行中1件のほかに8件までで、超過は`ResourceExhausted`で拒否する（電波に出る前に拒否されるため、失敗した購読はpersistent subscriptionのレコードにも残らない）。
5. **切断時は、その接続のqueue済みopを失敗`GattResult`として配送してから捨てる。** 黙って消すとアプリが永遠に来ないcallbackを待つ。GATT op実行中の`disconnect()`はrejectせず遅延実行する——rejectしてfalseを返すと、切断を要求した直後のアプリに「まだ繋がっている」と読めてしまう。要求時点でそのconnectionのqueue済みopを落とすのは、`update()`が`pumpGattQueue()` → `drainPendingDisconnects()`の順に走るため、キューを残すと毎回次のopが始まって切断が飢餓するから。
6. 最小Discoveryは既知Service/Characteristic UUIDを指定して存在とpropertyを確認する軽量経路として維持し、全列挙とは別に持つ。discovery snapshotは**接続ごと**に保持し（初回discoveryで確保、切断で解放）、ある接続のdiscoveryが他接続のsnapshotを追い出さない。
7. **希望MTUは`begin()`前に23〜517で設定する。** Notification/Indication payloadの上限は`mtu - 3`とし、backendによる黙示的な切詰めを避けるため超過を送信前に拒否する。broadcast送信ではactiveな全Peripheral Connectionの最小値を使う保守的な判定とし、接続指定送信では対象接続のMTUだけを見る。MTU交換は両役割ともグローバルGAPイベント（`BLE_GAP_EVENT_MTU`）で追跡し、Central側で接続確立後に完了する交換も拾う。
8. **Server送信はCentral clientと同じく内部FIFOへ積み、送信中でもrejectしない。** `EspBleGattSendResult`に`connectionId`（0=broadcast）を持ち、`notify(connectionId, …)`は`ble_gatts_notify_custom()`で単一接続へ送る。`indicate(connectionId, …)`は対称性のため用意するが、同梱backendのindication確認が`m_semaphoreConfEvt`（characteristic単位・private）で待つ設計のため接続単位の確認応答を安全に取り出せず、確認付きbroadcastパスへ委譲する。接続単位のindicate確認はbackend側が公開するまで見送る。
9. **コアGATT callbackは「primary 1（`on*`）＋listener 複数（`add*Listener` / `removeGattListener`・`removeListener`）」の多observerモデルとする。** `on*`は従来どおり単一primaryを差し替えるだけなので後方互換。これによりprofile helper（MIDI等）が単一スロットを独占せず、appは同一イベントを合成観測できる。配送はlock外でsnapshotをprimary→登録順に実行し、callback内の登録変更は次イベントから反映する。listener idはowner単位で単調発番。connection系イベント（`onConnected`等）はprofileが独占しないため単一slotのまま。
10. Peripheral向けイベント（Server書込み・購読変更・HID Output Report）でConnection IDを解決できない場合は、無効IDのまま配送せずdropしてカウントする。
11. `connect()`のtimeoutは`update()`が経過時間を監視して打ち切る。同梱NimBLEは接続失敗を自前で数回リトライするため（`BLE_GAP_EVENT_REATTEMPT_COUNT`）、`ble_gap_connect()`のduration引数だけでは指定時間を守れない。
12. **persistent subscriptionは既定onとする。** `subscribe()`成功時にpeer address＋service UUID＋characteristic UUIDで記録し、同一addressへ再接続した`Connected`処理で自動再購読する。復元はUUID指定（handleは再接続ごとに変わる）。registryは16件固定で、満杯時は既存を保持して新規のみ無視し、`droppedPersistentSubscriptionCount()`で観測できる。安定したpeer address（bond済みidentity、public、static random）を前提とする。`persistentSubscriptions=false`で手動管理へ切り替えられる。
13. **複数同時接続に公式対応する。** 上限は同梱NimBLE controllerの`CONFIG_BT_NIMBLE_MAX_CONNECTIONS`（esp32s3で3）で決まり、ライブラリの`ConnectionCapacity`（4）はslot数であって保証数ではない。auto-reconnectは`setAutoReconnect(bool)`（既定off）で、想定外の切断時に同一addressへ2秒間隔でretryし、`disconnect()`は意図的切断として対象から除外する。

## Security

1. Securityは`begin()`前の設定で有効化する。LE Secure Connectionsは常に有効で、Just Works（No Input / No Output）から段階的に方式を選ぶ。完了は同期戻り値ではなく値イベントとして`update()` contextへ配送する。
2. Connection snapshotはencrypted、authenticated、bonded、encryption key sizeを保持する。Just Worksの成功時はencryptedかつbondedだがauthenticatedではない。
3. GATT Characteristic定義にencrypted / authenticated read/writeを持たせ、NimBLEのATT permissionで強制する。
4. Bond列挙・特定削除・全削除は同梱NimBLE storeを使う。削除はactive Connectionがない場合だけ許可する。`bond(index)`はmutableなbond store上のsnapshot indexアクセスで、削除・追加により呼び出し間で並びが変わりうるため、特定削除は`deleteBond(const EspBleBond &)`がaddressで対象を特定する。
5. MITM方式は静的6桁passkey（DisplayOnly / KeyboardOnly）、実行時passkey入力（`providePasskey()`）、Numeric Comparison（DisplayYesNo＋MITM、`onNumericComparison()` / `confirmNumericComparison()`）に対応する。**stack callbackは回答まで同期でhost taskをblockする**ため、回答は30秒以内に返す必要があり、`loop()`を長くblockする処理と併用できない。`ble_sm_inject_io()`が`BLE_GAP_EVENT_PASSKEY_ACTION`のcontextを要求するbackend由来の制約で、回避できない。
6. passkey表示イベントのConnection IDは、Peripheral接続では自前の広告コールバックで`conn_handle`が取れるため正確。Central接続は「最初の未暗号化Connection」の推定となり、複数同時Pairingでは誤ったConnectionを報告しうる（STATUSの制限に記載）。なお`ble_gap_event_listener_register()`のグローバルリスナには**`PASSKEY_ACTION`が届かない**（`ENC_CHANGE`や`MTU`は届く）ため、接続コールバックで受ける必要がある。

## HID

1. **Device入口はprofileごとに分け、構成したものを1つのHID Serviceへ合成する。** `hidKeyboard()` / `hidMouse()` / `hidConsumerControl()` / `hidSystemControl()` / `hidGamepad()` / `hidVendor()` / `hidCustom()`。共通マネージャがHID/DIS/Battery登録、Report Map合成、Report characteristic、Report ID別CCCD、暗号化permission、notify routingを一元管理する。
2. **Report IDはEspUsbDevice/EspUsbHostと同じ固定値**（keyboard=1、mouse=2、gamepad=3、consumer=4、system=5、vendor=6）とし、利用者configから除く。構成順で決めると、profileを1つ足すだけで他の番号が動く。
3. Report payload自体にReport IDを含めない（Report MapとReport Referenceが持つ）。
4. Host入口は`hidHost()`へ集約し、Report Mapで識別した全対応Input Reportを購読して種別別callbackへ配送する。event共通baseは`connectionId` / `reportId` / raw bytesを持つ`EspBleHidReport`。
5. **Host入力の主境界は文字イベントではなく256-bit usage snapshot**（`EspBleHidKeyboardState`）とし、EspUsbHostとESP32KeyBridgeの入力境界へ揃える。modifier usagesもbitmapへ含める。切断時はheld usageの全release snapshotを配送し、bridgeにstuck keyを残さない。
6. **6KRO判定はバイト列一致ではなくReport Map parserで行う。** keyboard Application collection内の「8×1-bit modifier(0xE0-0xE7) variable入力＋6個以上の8-bit array入力」を持つreportを探す。長さだけで推測しない。Report IDなしのkeyboardも受理する。長さが期待と異なるInput Reportは解釈せず`invalidInputReportCount()`で数える——「Discovery成功なのにキーが来ない」の診断手段になる。
7. ErrorRollOver等のphantom report（key slotにusage 0x01-0x03）は一般的なHost同様に無視し、直前のkey状態を維持する。
8. `onKeyboard()`は同一report内の変化をpress（usage昇順）→release（usage昇順）の順で配送する。一般的なOS Hostのchord処理（release先行）とは順序が異なるが、主境界はusage snapshotなので影響しない。順序に依存する用途はraw usage境界を使う。
9. **NKROはEspUsbDeviceと同じmodifier 1 byte + 224-bit usage bitmap**とし、`enableNkro()`は`configure()`前に選ぶ。Hostは6KRO / NKROを同じsnapshotへ正規化する。29-byte notificationのため`preferredMtu`は32以上が必要で、**満たさない場合は`begin()`が拒否する**——黙ってMTUを引き上げてアプリの設定を隠すことも、毎回notifyが失敗する状態を作ることもしない。
10. keymap変換はEspUsbHostのUnicode 4-plane表現に揃える（`uint16_t KEYCODE_TO_UNICODE_XX[N][4]`＝無shift/Shift/AltGr/AltGr+Shift）。`espBleUsageToUnicode()`がAltGr層選択と文字ペア判定CapsLockを行い、`espBleUsageToAscii()`はLatin-1 wrapperとして維持する。
11. security有効時はHOGPのSecurity Mode 1 Level 2に従いHID Serviceの全attributeへ暗号化必須permissionを付け、暗号化されていないlinkへInput Reportを送らない。Device Information / Batteryは識別用途のため暗号化必須にしない。
12. Input Report / Battery Levelの通知は、GAP subscribeイベントから追跡したCCCD購読状態を持つ接続にのみ送る。購読者がいない場合`sendReport()`は失敗を返し、`setBatteryLevel()`は値更新のみ行う。
13. `setKeyboardLeds()`は同期`bool`を返すprofile helperとする。Write Without Responseを優先してATT応答を待たず、呼び出しtaskをblockしない。戻り値は受理を表し、配達確認はしない。汎用GATT側の非同期Resultへは統一しない。
14. **HID Hostの再Discoveryは`setAutoRediscover(bool)`（既定off）でopt-in自動化する。** HID Hostは汎用subscription registryを使わないためpersistent subscriptionの対象外で、代わりにdiscovery成功したpeer addressを記憶して再接続後のsecurity確立で`discover()`を再発行する。アプリが手動`discover()`を呼んでいればskipして二重discoveryを防ぐ。既定offなので「security完了後に明示`discover()`」の規範は据え置く。
15. Boot Protocol（Protocol Mode 0x2A4E＋Boot Keyboard Input/Output）は`EspBleHidKeyboardConfig::bootProtocol`によるopt-inで**既定off**とする。多くのHOGP HostはReport Protocol Modeで足り、characteristicが増えると全HostのDiscoveryが膨らむ。対象はkeyboardのみで、mouseのboot report（0x2A33）は非対応。
16. `EspBleHidKeyboardState`はESP32KeyBridgeの`InputAdapter::keys()`へ変換なしで写像でき、LockStateは`setKeyboardLeds()`で返送できる。

## BLE MIDI

1. profile helperは既存の公開GATT API（`gattServer()`とClient側の`discover` / `subscribe` / `onNotification` / `writeCharacteristic`）の上に薄く実装する。raw NimBLE登録（HID Device managerの方式）は使わない——MIDI Serviceは単一characteristicで複雑なGATT tableを持たないため公開APIで完結できる。
2. APIはUSB姉妹ライブラリに合わせる。`EspBleMidiDevice`は`EspBle &`参照で構築し`noteOn` / `noteOff` / `controlChange` / `programChange` / `polyPressure` / `channelPressure` / `pitchBend`を持つ。`EspBleMidiHost`は`onMidiMessage`と`sendNoteOn`等を持つ。`EspBleMidiMessage`は`EspUsbHostMidiMessage`と同じフィールド構成。
3. timestampは`millis() & 0x1FFF`（13-bitミリ秒clock）から生成する。送信はrunning status圧縮を行わず全メッセージに完全なstatusバイトを付ける（あらゆる準拠receiverが受理できる保守的なencode）。受信側はrunning statusをdecodeする。
4. 大きなSysExは`EspBleMidiSysExEncoder`でBLEパケットへ分割し、送信完了イベント駆動で1パケットずつ送る非同期queueとする。BLE送信は同時1件なので完了イベント駆動が排他制約と整合する。SysEx進行中は単発メッセージ送信をfalseで拒否してwire上のstreamを乱さない。1メッセージ上限は320 byte。
5. MIDI helperは必要な汎用GATT callbackへlistenerとして登録し（`begin()`はremove-before-addで重複防止）、単一primaryを独占しない。

## 標準Service

1. **標準Serviceに専用クラスを置かない。** 標準の側にあるのはUUIDとバイト並びの決まりだけで、GATTの仕組みとしては独自Serviceと違わない。専用クラスを増やすと仕様の一部だけを実装した抽象がその数だけ増える。公開GATT API上のexample＋Peerテストで対応する。

   例外はHIDとBLE MIDIの2つ。Report Descriptorという別の記述言語や13ビットタイムスタンプ・running statusのように「UUIDとバイト並び」で終わらず、**間違えてもエラーにならず「認識されない」「音がずれる」という形で失敗する**ため、抽象を置く価値がある。判断の余地がない変換（IEEE-11073 medical float、CGMのE2E-CRC）はヘッダで共有する。
2. Control Point手続き（Glucose RACP、FTMS Control Point）は「Client write→Server measurement notify→Server応答indicate」の順に、`onSent`駆動で順次実行する。これは送信が同時1件だからではなく**配送順序の保証**として維持する。独自profileのControl Pointにも応用できるパターン。
3. indicationは1接続あたり同時1件のみ在中できる（confirm待ち）。連続するControl Point応答は各confirmを待つ必要がある。

## テスト方針

1. **Peerテストを補助的なsmokeではなく主要な自動テストとする。** BLEは接続・切断・Discovery・購読・Security・Bondingが複数の非同期イベントにまたがるため、実機なしでは仕様を固定できない。
2. 常設ESP32-S3 2台を`s3_peer_host` / `s3_peer_device` profileで使う。この名前にBLE roleの意味を持たせない。親側sketchをCentral、`peer_device/`側をPeripheralに固定し、役割を交換しない。
3. **2台で成立するものは自動テスト、3台以上が必要なものはマニュアルテストとする。** 可能なら2台での自動テストが望ましい。3台前提のscenarioは`tests/manual/`へ置き、port未設定時は自動skipする。
4. **Peerの一方は可能な範囲で同梱BLE低レベルAPIの直接実装にし**、EspBle同士だけの自己整合テストにしない。将来は兄弟ライブラリ`EspBleBluedroid`を相手にした相互接続テストを追加する——スタックそのものが異なるため、より強い検証になる。
5. 実装だけでは完了とせず、対応するexampleとunit/build/Peerテストを同時に更新する。

## 文書構成

1. 公開APIと文書はBluetooth LEの標準用語を基本とし、Central/Peripheral、GATT Client/Server、HID Host/Deviceを同一視しない。stack ownerは役割中立の`EspBle`とする。
2. examplesの変数名は役割の明確さを優先する。複数roleが登場する場合は`hidKeyboardHost` / `hidKeyboardDevice`のように明示し、単一roleで自明なexampleでは短い名前を許容する。
3. **ガイドはGAP → セキュリティ → GATT → UUID → HID → BLE MIDIの章構成とする。** SMPはGAP・GATTと並ぶ独立した層であり、読者の作業順も「つながる → どこまで信頼するかを決める → 属性に要求を書く」であるため、セキュリティをGAP章の一節に押し込まない。役割分担はセキュリティ章がリンク単位の方針、GATT章が属性単位の要求（`encryptedRead`など）。
4. **「後述」で概念説明を飛ばさない。** examplesと同じく、ガイドもその場で完結して読めることを優先する。
5. **概念説明はガイドに一本化する。** 二重に持つと必ず食い違う（実際に同一UUID重複の記述が食い違っていた）。`examples/README`はガイドへの対応表を持つ。
6. **利用者が読む文書は日本語と英語を同期させる。** 対象は root `README`、`docs/README`、`docs/GUIDE_BLE_BASICS`、`docs/STATUS`、`docs/FEATURE_MATRIX`、`docs/RELEASE_CHECKLIST`、`examples/README`と各example README。設計・計画文書（`API_DESIGN` / `CORE_DESIGN` / `DECISIONS` / `REQUIREMENTS` / `TERMINOLOGY` / `HID_*_SPEC` / `PLAN_*` / `TEST_PLAN`）は日本語のみとし、`README.md`にその旨を明記する。
7. **過去の経緯や完了した作業計画は残さない。** 残すのは現在成立している仕様と、その理由だけ。examplesも最終仕様のみを書き、制限があるときはなぜできないのかを書く。上書きされた判断、撤去したラッパの挙動、完了した是正計画、未提出のupstream報告案はいずれも文書に置かない（必要ならgit履歴から辿る）。

8. **同じ事実を2箇所に書かない。** 使用例はヘッダとexampleとguideが持ち、設計文書には書かない——三重に持つと必ず食い違い、実際にAPI_DESIGNのサンプルが古い署名のまま残っていた。
9. exampleとテストの`sketch.yaml`のfqbnは`esp32:esp32:esp32s3`（オプション指定なし）に統一する。IDEでボードを選んだままの既定値と一致し、`tools/version_matrix.py`のfqbnとも一致する。

## 優先順位候補

1. その他の現役標準Service（必要になった時点で）
2. その他Connectionlessデータ（現役かつ検証容易なものに限る）

候補は採用決定ではない。ユースケース、実機、Peerテスト方法が揃った機能だけを正式スコープへ移す。

## 未確定

- Arduino-ESP32の最小対応版と更新ポリシー（`core-matrix.yml`が生成する`docs/COMPATIBILITY.<version>.md`で計測して確定する。ローカル実行はsketchを書き換えて環境を汚すためCIで回す）
- ESP32-S3以外のbuild matrix（`board-matrix.yml`が生成する`docs/BOARDS.<version>.md`で計測。NimBLE不在のBluedroid既定coreはコンパイル時`#error`で拒否する）
- HID Hostで追加対応するReport Mapの優先順位
- public object handleの表現（値型、index+generation、参照class）
- Boot Protocolの市販Host（BIOS等）との互換性と、Mouse Boot Report 0x2A33の対応要否（manual interoperabilityで確認する）
