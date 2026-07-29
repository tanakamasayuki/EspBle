# 設計負債と是正計画（pre-1.0.0）

この文書は、1.0.0前に「あるべき姿」へ揃えるための**設計起因の制約**と、その是正計画・進捗を追跡します。対象は自分たちのアーキテクチャ/API設計に起因し修正可能なものだけです。backend（Arduino-ESP32同梱NimBLE）由来のハード制約や、意図的なトレードオフは「対象外」として区別し、混同しないために併記します。

1.0.0前のため公開APIの破壊的変更は許容します。各是正は実装だけで完了とせず、対応するexampleとunit/Peerテストを同時に更新します。確定した設計判断は[DECISIONS.ja.md](DECISIONS.ja.md)へ移します。

## 背景（一本の筋）

汎用GATT **クライアントのread/write**パスだけが成熟した扱い（呼び出しは自動でFIFO queueに積まれ「already in progress」で失敗しない、接続ごとのdiscovery cache、接続ごとのrouting）を受け、**兄弟パス3つが取り残されている**のが根本テーマです。

- HID Host のGATT操作 → 別タスク＋同期 `gattOperating` gate（queue不使用）
- GATT **Server送信** → 単一接続前提・single-in-flightで**reject**（queue無し）
- **イベント配送** → 単一スロットcallback（HID Hostだけが多listener registryを持つ）

「あるべき姿」は、ATTに触れる全操作を**pump式FIFO queue**へ、全callbackを**多listener registry**へ収束させること。コードの既存パターン（クライアントqueue、HID Host registry、MIDI Hostの `discoverCharacteristic()` 利用）が既に正解を実証済み。

## 分類

- **是正対象**: 自分たちの設計起因で修正可能。
- **意図的**: 妥当なトレードオフとして維持。
- **対象外**: backend由来のハード制約（自分たちでは直せない）。

※ ファイル行番号は着手時点の目安。実装で移動する。

---

## 是正対象（優先度順）

### クラスタA — GATT実行エンジンの二重化【方法2・完了】

HID Host の `discover()` が汎用queueエンジンに乗らず、別経路になっていた。

- `EspBleHidHost::discover()` が `gattOperating` を同期チェックしfalse＋専用タスク `discoveryTaskEntry` を直接起動していた（旧 [EspBle.cpp:5630](../src/EspBle.cpp#L5630)）。
- generic bookkeeping（`gattStartMilliseconds`/`gattTimeoutMilliseconds`/`gattOperation`）を設定せず `gattOperating=true` にするため、`expireGattOperation()` が**古いUUIDで偽のtimeout GattResultを出す危険**があり、完了時に `pumpGattQueue()` を呼ばずqueue済みgeneric opが次の `update()` まで待っていた。
- 結果、`Connected` で発火するpersistent-subscription自動再購読と `discover()` が競合し false を返す（`lifecycle_stress` の `test_reconnect_cycles_do_not_leak_heap` 顕在化）。

**是正内容（実装済み）**: `EspBleGattOperation::HidDiscover` を追加。`discover()` は他op同様 `startGattOperation()` で queue に積み**常に受理（true）**を返す（真のエラー＝未接続・確保失敗でのみfalse）。`pumpGattQueue()` が HidDiscover を取り出したら `EspBleHidHost::runQueuedDiscovery()` 経由でdiscovery workerを起動し、単一の `gattOperating` slotを共有して generic op と直列化する。`expireGattOperation()` は HidDiscover を除外（workerが自前で完了させる）。これで race・偽timeout・no-pump が解消。

- **エンジン統合の形**: discoveryの実処理はHID固有の多段blocking処理のため、workerタスク自体はHID側（`runQueuedDiscovery`）に残す。ただし起動は必ず単一のqueue経由で、`EspBleImpl` にHID内部を持ち込まない層分離を保つ。「ATTに触れる全操作が単一queue・単一slotで直列化される」という不変条件は満たす。
- **setKeyboardLeds / sendVendorReport は同期のまま維持（一度queue化を試みたが実機で退行）**: 正しさ基準で両者もqueue化を実装したが、実機で **HID input が壊れた**（queue済みwrite直後から入力notificationが届かなくなる。`hid_keyboard_host` はvendor write後のkeyboard入力、`hid_keyboard_nkro` はLED write後のrelease-all入力で失敗、`hid_boot_keyboard` へ波及）。原因は同梱backendで、worker task上のoutput report writeがHID input report購読と同一client上で並行するとnotification配送を壊すため。`hid_custom`（HID input購読を持たない汎用client）では出ない。したがってこれらは同期WWR fire-and-forget（`gattOperating` gateでworkerと直列化、busy時false）を正解として維持する。DECISIONS #6 の判断がこの実機挙動で裏付けられた形。discover() は元々 `onDiscovered` の非同期契約なので統合は安全（discovery＋初期入力は実機で正常）。

**破壊的変更**: `discover()` の戻り値が「busy時false」→「常に受理true」のみ。
**テスト影響**: `lifecycle_stress`（`test_reconnect_cycles_do_not_leak_heap` が再接続時の自動再購読とdiscoverの共存を検証する回帰テストになる）。HID系テストは同期writeへ戻したため元のまま。
**状況: 完了（discover統合・実機確認済み。HID Host一式＋`lifecycle_stress`がグリーン）**

### クラスタB — GATT Server送信が単一接続・single-in-flight reject

クライアント側と同じ「同時1件」制約を逆方針で実装しており、multi-connectionが正式対応済みの今、最大の設計ギャップ。

- 接続指定の `notify()`/`indicate()` が無い（[EspBle.h:715](../src/EspBle.h#L715) 付近）。`EspBleGattSendResult` に `connectionId` が無く、どの購読者の送信が失敗したか特定できない（[EspBle.h:363](../src/EspBle.h#L363) 付近）。送信は全購読者へのbroadcast（[EspBle.cpp:3993](../src/EspBle.cpp#L3993) / backend notify [1754](../src/EspBle.cpp#L1754)）。
- `sending` 中は**queueせずreject**（[EspBle.cpp:4038](../src/EspBle.cpp#L4038)）。characteristic AとBの同時notifyもできず、Glucose RACP / FTMS control point / MIDI SysEx は全て `onSent` 駆動の手動シーケンスを強いられる（クライアント側で撤廃したのと同じfootgun）。
- MTU判定が**全peripheral接続の最小payload**（[EspBle.cpp:4013](../src/EspBle.cpp#L4013)）。無関係な低MTU接続のせいで、高MTU購読者への大きなnotifyが拒否される。

**是正内容（実装済み・scope A）**: クライアントと同じ内部送信FIFO（容量8、`EspBle::pumpSendQueue()` が `update()` からpump）を追加し、`notify()`/`indicate()` は**rejectせず常にqueue**（真のエラー＝未接続・未登録・非notifiable・queue満杯でのみfalse）。`EspBleGattSendResult` に `connectionId`（0=broadcast）。`notify(connId, …)` を追加し、backend低レベルの `ble_gatts_notify_custom(connHandle, char->getHandle(), om)`（HID Device送信と同型）で**接続指定送信**、MTU判定も**対象接続のMTUのみ**。broadcast送信は従来どおりbackend `notify()` 経由でmin-MTU判定を維持。

- **indicateの接続指定はbackend制約でbroadcast確認パスに委譲**: 同梱backendのindication確認は `m_semaphoreConfEvt`（characteristic単位、private）で待つため、接続単位の確認応答を安全に取り出せない。`indicate(connId, …)` はAPI対称性のため用意するが確認付きbroadcastパスで送る（単一購読者ならその接続、resultの `connectionId` は要求値を反映）。**クラスタAでHID writeをqueue化して実機退行したのと同種の「wrapper確認machinery自前化」リスクを避ける判断**。scope選択の記録。
- **profileのonSent順次実行は撤廃せず、コメントを修正**: Glucose RACP / FTMS Control Point のonSent駆動シーケンスは「send単一in-flightのworkaround」ではなく、**measurement配送完了を待って完了応答をIndicateする配送順序保証**という独立した妥当性を持つ。よって撤廃せず、誤解を招く「BLE送信は同時1件」コメント（GlucoseServer/glucose/fitness_machine の各sketch＋README）を実態（送信はqueueされる／onSentは順序保証の意図的選択）へ修正した。

**破壊的変更**: `EspBleGattSendResult` に `connectionId` フィールド追加、send系の挙動（reject→queue）。`send()` privateシグネチャにconnectionId追加。
**テスト影響**: `notify_indicate`（connectionId出力・queued burst・接続指定notifyの検証を追加）。`glucose` / `fitness_machine` はコメントのみ変更で挙動不変。
**状況: 完了（scope A。要実機再確認）**

### クラスタC — コアcallbackが単一スロット（HID Hostだけ多listener）

- client/server のコアcallback（`onNotification` / `onCharacteristicDiscovered` / `onSubscribed` / `onCharacteristicWritten` / server `onWritten` / `onSubscriptionChanged` / `onSent`）は上書き型の単一スロット（[EspBle.h:1376](../src/EspBle.h#L1376) 付近、server [754](../src/EspBle.h#L754) 付近）。
- HID Host は多listener registry（`add*Listener`/`removeListener`、[EspBle.h:998](../src/EspBle.h#L998) 付近）を持つ。
- 結果、MIDI helperが単一スロットを独占し（headerに「MIDI利用時は自分でこれらcallbackを使うな」と明記）、2つのprofile helperや「app観測＋profile」の併用ができない。

**是正内容（実装済み）**: 再利用可能な `EspBleCallbackList<Callback>`（primary 1 + listener 4、配送時snapshot・invokeはlock外・登録順、ownerがmutex提供）を導入し、コアGATT callbackを全て置換。

- **client（9）**: `onCharacteristicDiscovered`/`Read`/`Written`、`onServicesDiscovered`、`onDescriptorRead`/`Written`、`onSubscribed`/`onUnsubscribed`、`onNotification`。`add*Listener`（`addNotificationListener` 等）＋ `removeGattListener(id)`。
- **server（4）**: `onWritten`/`onDescriptorWritten`/`onSubscriptionChanged`/`onSent`。`add*Listener`＋ `EspBleGattServer::removeListener(id)`。
- listener idはowner単位で単調発番（実質衝突なし）。dispatchはprimary→listener登録順。
- **MIDI helper は `on*` 独占をやめ全て `add*Listener` へ移行**（device: written/subscriptionChanged/sent、host: notification/discovered/subscribed/written）。primaryスロットと残りlistener枠がappに開放され、「app観測＋MIDI」が併用可能に。`add*Listener` は非idempotentなので `begin()` はremove-before-addで重複登録を防止。

**破壊的変更なし（追加API）**: `on*` は「primaryスロット」として従来どおり動作（単一observerのsketchは無改修）。`add*Listener`/`removeGattListener`/`removeListener` を追加しただけ。
**テスト影響**: `notify_indicate`（client `addNotificationListener` と server `addSentListener` の第2observerがprimaryと同時発火することを検証）。`midi_device`/`midi_host` はhelperがlistener経由になっても挙動不変（コンパイル確認済み）。

**追加検証（実施済み）**: 公開APIとテストの突き合わせで、`EspBleCallbackList` と `add*Listener` / `removeGattListener` / `removeListener` に**Peer・unit・手動・examplesのいずれからも呼び出しが無い**ことが判明した（`notify_indicate` は第2observerの発火だけを見ており、解除と上限は未検証だった）。Peerテスト `multi_listener` を新設し、primary＋listener 2件への同時配送、**1件だけの解除**（残りとprimaryに影響しない）、未登録idの削除が`false`を返すこと、上限4件で追加が拒否され既存を追い出さないことを、`EspBleGattServer` 側と `EspBle` 側の両方で実機検証した。

**状況: 完了（実機検証済み。`multi_listener` / `notify_indicate`）**

### 小粒（自己完結）

1. **persistent-subscription registryの無言overflow**: `free==nullptr` で黙って記録しない（容量16）。既定onのため、再接続時に復元されないことにアプリが気づけない。→ **完了**: `droppedPersistentSubscriptions` カウンタを追加、overflow時に加算し、公開 `EspBle::droppedPersistentSubscriptionCount()` で露出（`droppedEventCount()` に倣う）。

**カウンタも実機検証済み**（Peerテスト `persistent_subscription_overflow`）。overflowには17件の異なる（peer, service, characteristic）が必要だが、centralのアクティブ購読表も16件で先に埋まるため1接続では到達できない。同じアドレスへ再接続しても自動復元がアクティブ表を埋め直すので同じ。**Peripheralが`end()`＋`begin()`でownAddressTypeをPublicから`RandomStatic`へ変え、別peerとして数えさせる**ことで2台のまま成立させた。17件目の`subscribe()`自体は成功し失われるのはレコードだけ——カウンタが必要な理由そのもの——であることも同時に確認している。**状況: 完了（実機検証済み）**
2. **切断時のqueue未purge＋GATT op中の `disconnect()` reject**: `removeConnection` が `gattQueue` を触らず、切断済み接続のqueue済みopが残って生存接続を遅延させる。`disconnect()` はGATT op中false。→ **完了**: `removeConnection` が `purgeQueuedGattOpsLocked(connectionId)` で当該接続のqueue済みopをdrop（generic opは失敗GattResultを配送して完了contractを維持、queued HidDiscoverはHID Host切断処理に委ねて静かにdrop、実行中opは無干渉）。`disconnect()` はGATT op中に**rejectせず deferred**（`ConnectionSlot::pendingDisconnect`＋`update()` の `drainPendingDisconnects()` でop完了後に実行）。**状況: 完了（要実機再確認）**
3. **NKROのMTU下限未引き上げ**: `enableNkro()` はフラグとreport長29のみ設定し `preferredMtu`（既定23）を触らないため、29-byte notifyが送信時に無言失敗。→ **完了**: ライブラリの明示エラー方針に合わせ、`begin()` で「NKRO keyboard configured かつ `preferredMtu < 32`」を `InvalidArgument` で拒否（無言失敗より明示エラー。silentにMTUを上書きしない）。**状況: 完了（要実機再確認）**

4. **`connect()` timeoutが効かない（cancel前提の設計が誤り）**: `cancelExpiredConnectAttempt()` は要求timeout経過時に `ble_gap_conn_cancel()` で backend の待ちを打ち切る設計だったが、**打ち切れない**。同梱wrapperのNimBLE `BLEClient::connect()` は `ble_gap_connect()` ではなくBluedroid互換層の `esp_ble_gattc_open()` を使うため、接続試行がホストの追跡するGAP手続きとして存在せず、`ble_gap_conn_cancel()` は `BLE_HS_EALREADY`(rc=2) を返して空振りする。`accept_list` Peerで、timeout 4000 ms 指定に対し `BLEClient::connect()` の戻りが実測 **31000 ms**（成功時は219 ms）であることを確認した。→ **完了**: cancel前提をやめ、**timeout経過時に試行を「放棄」する**方式へ変更。失敗イベント（`EspBleError::Timeout`）を即座に配送し、`connecting` を解放してアプリが直ちに再接続できるようにする。放棄されたworkerは戻ってきた時点で結果を破棄して自分のclientをretireし、遅れて成立した接続は `ClientCallbacks::onConnect` で slot に載せずに切断する。`end()` は放棄済みworkerの終了も待ってからdeinitする。`accept_list` Peerで、失敗が4秒で返りその直後の接続が（旧workerがbackend内でブロックされたまま）成立することを実機確認。→ **その後、接続自体を `ble_gap_connect()` へ移してcancelが本当に効くようになった**（[PLAN_GUIDE_REVAMP.ja.md](PLAN_GUIDE_REVAMP.ja.md) Phase 4b S5）。放棄機構は削除し、期限が来たら `ble_gap_conn_cancel()` で打ち切る。なお同梱NimBLEは接続失敗を自前で数回リトライするため（`BLE_GAP_EVENT_REATTEMPT_COUNT`）、`ble_gap_connect()` のdurationだけでは指定時間を守れず、期限の管理は引き続き `update()` 側で行う。**状況: 完了**

5. **handle指定の `readDescriptor()` / `writeDescriptor()` が無い**: characteristic側には同一UUIDの重複に届くようhandle overload（`readCharacteristic(id, handle)` ほか）を用意したのに、descriptor側は `readDescriptor(id, serviceUuid, characteristicUuid, descriptorUuid)` のUUID指定しかない。同一UUIDのcharacteristicが並ぶとき、そのどれのdescriptorなのかをUUIDの組では指定できない。**HIDのReport Reference（0x2908）はまさにその状況**——Report characteristicは全部0x2A4Dなので、アプリからは「このReportのReport Referenceを読む」ができない。ライブラリ内部（HID HostのDiscovery）は `espble_raw` でhandle直指定して読んでいるので、公開APIだけが届いていない。`EspBleGattDescriptorInfo` は既に `characteristicHandle` を持っており、Discoveryの結果からhandleは取れるので、必要なのはoverloadの追加だけ。**状況: 未着手（Phase 10の `hid_custom` 拡張中に発見。当該テストはcharacteristicのflags（Write Without Responseの有無）でoutputとfeatureを見分けたため回避できている）**

### より大きめ（任意・クラスタA完了が前提）

- **HID Host の再接続時auto-rediscover**: HID Host は汎用subscription registryを使わないため `persistentSubscriptions`＋`setAutoReconnect` の恩恵を受けず、再接続時にアプリが手動で `discover()` 再実行（かつsecurity完了後）を要した。→ **完了（opt-in）**: `EspBleHidHost::setAutoRediscover(bool)`（既定off）。discovery成功したCentral peerのaddressを記憶し、再接続後のsecurity確立イベントで自動的に `discover()` を再発行する。アプリが `onSecurityChanged` で従来どおり手動 `discover()` を呼んでいても、その接続に既にHID discoveryがqueue/実行中なら自動側はskipして二重discoveryを防ぐ（`EspBle::hasPendingHidDiscover()` で判定）。記憶集合は最大 `MaxRediscoverPeers`(4)、`resetBackend()` でクリア、loop task専用で無lock。`setAutoReconnect`＋`persistentSubscriptions` と併用でHID再接続がハンズオフになる。**状況: 完了（要実機再確認）**
- **op毎のタスク生成をやめ、エンジン毎の常駐workerへ**: `pumpGattQueue`（op毎6144B）、HID discovery（16384B）、server send がop毎に `xTaskCreate`。メモリ逼迫時にResourceExhausted。→ **見送り（意図的）**: 常駐worker化は「メモリ逼迫時の `xTaskCreate` 失敗回避」（全実機テストで未観測の理論上事象）と引き換えに、idle時・未使用機能でも常駐task分（HIDは16KB級）のメモリを**常時確保**する。ライブラリとしては現行の「都度生成・都度解放（idleコスト0・未使用機能は未確保）」の方が一般ケースで有利なため、常駐化は行わない。**状況: 見送り（意図的トレードオフ、[意図的]節参照）**

---

## 意図的（維持する）

- 単一 `lastError_`/`setError`（[EspBle.cpp:8141](../src/EspBle.cpp#L8141) 付近）。単一loop-task前提（DECISIONS #19）。
- `String` value container（DECISIONS #20）。将来 `EspBleBytes` 移行余地は残す。
- `ConnectionCapacity=4` vs backend最大3（DECISIONS 再接続#4）。slot数であり保証数ではない。
- operation id / 強制cancel無し（DECISIONS #19）。ただし上記「切断時purge」は別扱いで是正。
- Boot Protocol既定off（discovery leak増幅回避、多くのHostはReport Protocolで足りる）。
- `update()` 駆動の単一スレッドdispatch（DECISIONS #17）。
- Glucose RACP / FTMS Control Point の `onSent` 駆動シーケンスは**配送順序保証として維持**（クラスタB完了後の再評価で、send単一in-flightのworkaroundではなく完了応答の順序保証だと確認。誤解コメントのみ修正済み）。

## 対象外（backend由来・修正不能）

- SMコールバック（passkey要求 / Numeric Comparison確認）が同期でhost taskを最大30秒block。`ble_sm_inject_io()` は `BLE_GAP_EVENT_PASSKEY_ACTION` を受けた文脈で答える必要があり、アプリの応答を待つ間はそこで止まる。
- ~~GATT client discoveryのheap leak（約2.6 KB/discovery）~~ → **該当しなくなった**。discoveryもread/write/購読もNimBLEホストAPIへ直接発行し、wrapperの `BLEClient` は一切使わない。HID Hostも同じ経路へ移行済み（[PLAN_GUIDE_REVAMP.ja.md](PLAN_GUIDE_REVAMP.ja.md) Phase 4b S1・S6）。`gatt_read_write` の `test_discovery_cycles_do_not_leak_heap` が8サイクルで実測する。
- client側MTU変更callback無し（接続時snapshotのみ）。
- Extended/Periodic Advertising、動的service追加、`connect()` のtimeout引数無視、最大3接続（同梱NimBLEビルド構成）。

> **解決済み（自前広告化）**: passkey表示 / Numeric Comparisonの接続attributionが「最初の未暗号化接続」の推定だった件は、Peripheral接続では解消した。`BLE_GAP_EVENT_PASSKEY_ACTION` を自前の広告コールバックで受けるため `conn_handle` が判る（Central接続はwrapperのsecurity callback経由のままで、そこは引き続き推定）。なお `ble_gap_event_listener_register()` のグローバルリスナには **PASSKEY_ACTION が届かない**（`ENC_CHANGE` や `MTU` は届く）ため、接続コールバックで受ける必要がある（[PLAN_GUIDE_REVAMP.ja.md](PLAN_GUIDE_REVAMP.ja.md) Phase 4b S3）。

> **解決済み（自前GATT Server化）**: Descriptor Write eventの接続ID欠落は、属性テーブルを`ble_gatts_add_svcs()`で自前に組むようにして解消した。ホストのaccess callbackは`conn_handle`を受け取るため、`EspBleGattDescriptorWrite::connectionId`を埋められる（[PLAN_GUIDE_REVAMP.ja.md](PLAN_GUIDE_REVAMP.ja.md) Phase 4b S2）。

> **解決済み（3.3.10 → 3.3.11）**: 3.3.10では `ble.begin()` のBTコントローラ初期化中に `ipc0` タスク（`CONFIG_ESP_IPC_TASK_STACK_SIZE=1024`）でスタック超過→再起動ループが**マージナルに**発生した（`btdm_intr_alloc → heap_caps_malloc` へISRがネストしスタックcanary watchpointが発火。`hid_boot_keyboard` peer で顕在化、バイナリ配置依存）。arduino-esp32 3.3.11でipcタスクスタックが拡張され根治（開発/テストは3.3.11で実施し、当該peerを含め全PASS）。

---

## 実行順

1. **クラスタA**（方法2・決定済み） — HID Host ATT操作を汎用queueへ統合。
2. **クラスタB** — Server送信を接続scope＋内部FIFO化。onSent手動pumpとmin-MTU回避を削除。
3. **クラスタC** — コアcallbackを多listener registryへ昇格。MIDI独占前提を撤廃。
4. **小粒3点** → **任意2点**。

各クラスタは独立してPeer/unitテスト更新を伴う。1→4をこの順で進める。
