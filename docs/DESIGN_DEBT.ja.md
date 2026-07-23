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

**あるべき姿**: `notify(connId, service, char, …)` / `indicate(connId, …)` 追加＋broadcast overloadは併存。`EspBleGattSendResult` に `connectionId`。クライアントと同じ内部送信FIFO（`update()` からpump）で「いつ呼んでもqueue」化。MTU判定は接続scope後に**対象接続のMTUのみ**で行い、保守的なmin-MTU回避を削除。

**破壊的変更**: `EspBleGattSendResult` 構造体、send系の挙動（reject→queue）。
**テスト影響**: `glucose` / `fitness_machine` / `midi_*`（手動onSent pumpの簡素化）、notify/indicate系Peer。
**状況: 未着手**

### クラスタC — コアcallbackが単一スロット（HID Hostだけ多listener）

- client/server のコアcallback（`onNotification` / `onCharacteristicDiscovered` / `onSubscribed` / `onCharacteristicWritten` / server `onWritten` / `onSubscriptionChanged` / `onSent`）は上書き型の単一スロット（[EspBle.h:1376](../src/EspBle.h#L1376) 付近、server [754](../src/EspBle.h#L754) 付近）。
- HID Host は多listener registry（`add*Listener`/`removeListener`、[EspBle.h:998](../src/EspBle.h#L998) 付近）を持つ。
- 結果、MIDI helperが単一スロットを独占し（headerに「MIDI利用時は自分でこれらcallbackを使うな」と明記）、2つのprofile helperや「app観測＋profile」の併用ができない。

**あるべき姿**: HID Host のlistener-registryパターン（配送時snapshot・mutexはcallback中保持しない・単一`on*`→登録順）をコアGATT client/server callbackへ昇格し、profile helperとappが競合せず合成できるようにする。

**破壊的変更**: callback登録モデル（`on*` 単一→`add*Listener` 併存）。
**テスト影響**: MIDI helper（独占前提の撤廃）、通知/購読系の広範なPeer。
**状況: 未着手**

### 小粒（自己完結）

1. **persistent-subscription registryの無言overflow**: `free==nullptr` で黙って記録しない（[EspBle.cpp:1443](../src/EspBle.cpp#L1443)、容量16 [1399](../src/EspBle.cpp#L1399)）。既定onのため、再接続時に復元されないことにアプリが気づけない。→ drop/overflowカウンタを露出（`droppedEventCount()` に倣う）。**状況: 未着手**
2. **切断時のqueue未purge＋GATT op中の `disconnect()` reject**: `removeConnection`（[EspBle.cpp:576](../src/EspBle.cpp#L576)）が `gattQueue` を触らず、切断済み接続のqueue済みopが残って他の生存接続を遅延させる。`disconnect()` はGATT op中false（[6853](../src/EspBle.cpp#L6853)）。→ removeConnectionで当該connectionIdのqueue済みopをdrop、disconnectは遅延/queue化。**状況: 未着手**
3. **NKROのMTU下限未引き上げ**: `enableNkro()`（[EspBle.cpp:4301](../src/EspBle.cpp#L4301)）はフラグとreport長29のみ設定し `preferredMtu`（既定23）を触らないため、29-byte notifyが送信時に無言失敗（MTU payload guard [4030](../src/EspBle.cpp#L4030)）。→ `enableNkro()` で実効MTU下限を32へ（or begin()でNKRO＋MTU<32を明示エラー）。**状況: 未着手**

### より大きめ（任意・クラスタA完了が前提）

- **HID Host の再接続時auto-rediscover**: HID Host は汎用subscription registryを使わないため `persistentSubscriptions`＋`setAutoReconnect` の恩恵を受けず、再接続時にアプリが手動で `discover()` 再実行（かつsecurity完了後）を要する。→ persistent-subscription相当のopt-in auto-discover（address記憶→再接続後にsecurity安定を待って再discovery）。**状況: 未着手**
- **op毎のタスク生成をやめ、エンジン毎の常駐workerへ**: `pumpGattQueue`（[EspBle.cpp:7833](../src/EspBle.cpp#L7833)、op毎6144B）、HID discovery（16384B）、server send がop毎に `xTaskCreate`。メモリ逼迫時にResourceExhausted。→ エンジン毎に常駐worker 1本でqueue消費。**状況: 未着手**

---

## 意図的（維持する）

- 単一 `lastError_`/`setError`（[EspBle.cpp:8141](../src/EspBle.cpp#L8141) 付近）。単一loop-task前提（DECISIONS #19）。
- `String` value container（DECISIONS #20）。将来 `EspBleBytes` 移行余地は残す。
- `ConnectionCapacity=4` vs backend最大3（DECISIONS 再接続#4）。slot数であり保証数ではない。
- operation id / 強制cancel無し（DECISIONS #19）。ただし上記「切断時purge」は別扱いで是正。
- Boot Protocol既定off（discovery leak増幅回避、多くのHostはReport Protocolで足りる）。
- `update()` 駆動の単一スレッドdispatch（DECISIONS #17）。
- MIDI SysEx / RACP の `onSent` 駆動pump は**クラスタBの症状**。B完了後にprofile側を簡素化する。

## 対象外（backend由来・修正不能）

- SMコールバック（passkey要求 / Numeric Comparison確認）が同期でhost taskを最大30秒block（NimBLEの `onPassKeyRequest()`/`onConfirmPIN()` がinline戻り必須）。
- passkey表示 / Numeric Comparisonの接続attributionが「最初の未暗号化接続」の推定（backend callbackにconn handle無し、DECISIONS Security#8）。
- Descriptor Write eventに接続ID無し（backendが非公開）。
- GATT client discoveryのheap leak（約2.6 KB/discovery）。
- client側MTU変更callback無し（接続時snapshotのみ）。
- Extended/Periodic Advertising、動的service追加、`connect()` のtimeout引数無視、最大3接続（同梱NimBLEビルド構成）。

> **解決済み（3.3.10 → 3.3.11）**: 3.3.10では `ble.begin()` のBTコントローラ初期化中に `ipc0` タスク（`CONFIG_ESP_IPC_TASK_STACK_SIZE=1024`）でスタック超過→再起動ループが**マージナルに**発生した（`btdm_intr_alloc → heap_caps_malloc` へISRがネストしスタックcanary watchpointが発火。`hid_boot_keyboard` peer で顕在化、バイナリ配置依存）。arduino-esp32 3.3.11でipcタスクスタックが拡張され根治（開発/テストは3.3.11で実施し、当該peerを含め全PASS）。

---

## 実行順

1. **クラスタA**（方法2・決定済み） — HID Host ATT操作を汎用queueへ統合。
2. **クラスタB** — Server送信を接続scope＋内部FIFO化。onSent手動pumpとmin-MTU回避を削除。
3. **クラスタC** — コアcallbackを多listener registryへ昇格。MIDI独占前提を撤廃。
4. **小粒3点** → **任意2点**。

各クラスタは独立してPeer/unitテスト更新を伴う。1→4をこの順で進める。
