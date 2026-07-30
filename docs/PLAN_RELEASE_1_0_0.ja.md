# 1.0.0リリース前タスクリスト

[PLAN_GUIDE_REVAMP.ja.md](PLAN_GUIDE_REVAMP.ja.md)（Phase 0〜11）が完了したため、残作業をここへ集約する。この文書はリリースまでの唯一の作業リストとし、完了した項目は削除せず結果を書き足す。

**現在の状態**: Peerテスト65 suite / 82 test、unit 7、example compile 91。src配下に`TODO`/`FIXME`は無い。

## 棚卸しで判明した文書の食い違い

リスト作成のために[DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md)と[STATUS.ja.md](STATUS.ja.md)を実テストと突き合わせた結果、**3件は作業ではなく記述の訂正**だった。再検証や再実装をしないよう先に分離しておく。

| # | 記述 | 実際 |
|---|---|---|
| C-1 | DESIGN_DEBT クラスタB（Server送信の接続scope化）が「要実機再確認」 | **検証済み。** `notify_indicate` がbroadcast時の`connectionId=0`、queueされた連続3件、接続指定notify（`id=1`）、indicateをすべてassertしている |
| C-2 | DESIGN_DEBT「HID Host の再接続時auto-rediscover」が「要実機再確認」 | **検証済み。** `hid_keyboard_host` が、2回目のsecurity確立で**sketchが`discover()`を呼ばない**（`HOST_RECONNECT_SECURITY`）にもかかわらずdiscoveryが走ることをassertしている |
| C-3 | STATUS「1.0.0前の残作業」項目1 = 「FEATURE_MATRIXのHID拡張のうち1.0.0に含めるものを実装する」 | **対象が空。** FEATURE_MATRIXに未実装のHID拡張は残っていない（USB由来機能は任意DescriptorのCustom HID・BLE MIDI・複数同時接続すべて対応済み）。今後の候補は[DECISIONS.ja.md](DECISIONS.ja.md)の「優先順位候補」にある2件だが、そこに明記のとおり**採用決定ではない** |

---

## A. 実装が必要（設計上の穴）

### A-1. handle指定の `readDescriptor()` / `writeDescriptor()` を追加する — ✅ 完了

**当時の状況**: characteristic側には同一UUIDの重複に届くようhandle overload（`readCharacteristic(id, handle)` / `writeCharacteristic` / `subscribe` / `unsubscribe`）があるのに、descriptor側は`readDescriptor(id, serviceUuid, characteristicUuid, descriptorUuid)`のUUID指定しかない。

**なぜ穴か**: 同一UUIDのcharacteristicが並ぶとき、そのどれのdescriptorなのかをUUIDの組では指定できない。**HIDのReport Reference（0x2908）はまさにその状況**——Report characteristicは全部0x2A4Dなので、アプリからは「このReportのReport Referenceを読む」ができない。handle overloadを用意した動機そのものがdescriptor側で満たされていない。ライブラリ内部（HID HostのDiscovery）は`espble_raw`でhandle直指定して読んでおり、公開APIだけが届いていない。

**完了内容**: `readDescriptor(connectionId, descriptorHandle)` と `writeDescriptor(connectionId, descriptorHandle, ...)`（pointer+length版・String版）を追加。`EspBleGattResult` に `descriptorHandle` を追加した（`handle` はそれを持つcharacteristic）。

実装の要点は**解決の順序**。ハンドル指定のdescriptor操作は、discovery snapshotから**descriptorを先に**引き、その `characteristicHandle` で持ち主のcharacteristicを解決する。逆順（UUIDでcharacteristicを引いてからdescriptorを探す）では、重複したcharacteristicのどれかを選べないので成立しない。UUID指定側も結果に解決済みの `descriptorHandle` を載せる。失敗時は要求ハンドルをそのまま返す——どの呼び出しの結果か分からなければ、非同期完了の意味がない。

`hid_custom` Peerを**HIDが本来宣言している方法**へ置き換えた: 各Report Reference（0x2908）をハンドル指定で読み、type byte（1=Input / 2=Output / 3=Feature）で役割を決める。従来のflagsによる代用は捨てず、「宣言されたtypeとflagsが一致すること」の照合として残した（Featureが応答付き書き込みのみであることの確認になる）。ゼロハンドルの拒否と存在しないハンドルの`NotFound`も検証済み。

文書は FEATURE_MATRIX・STATUS・GUIDE_BLE_BASICS（いずれも日英）、DESIGN_DEBT小粒5、TEST_PLAN項目51を更新。`keywords.txt`は`readDescriptor`/`writeDescriptor`が既に登録済みでoverloadは同名のため変更不要（struct fieldは元から列挙していない）。

出典: [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) 小粒5（Phase 10の作業中に発見）

---

## B. 実装済みだが未検証（テストが必要）

いずれも2台の常設構成で自動テストにできる見込み。[MEMORY: 2台なら自動テスト]の方針に従い、マニュアルテストは作らない。

### B-1. 切断時のGATT queue purgeと、GATT op中の `disconnect()` の遅延実行 — ✅ 完了（是正あり）

**実装内容**: `removeConnection`が`purgeQueuedGattOpsLocked(connectionId)`で当該接続のqueue済みopをdropする（generic opは失敗`GattResult`を配送して完了contractを維持、queued HidDiscoverはHID Host切断処理に委ねて静かにdrop、実行中opは無干渉）。`disconnect()`はGATT op中に**rejectせずdeferred**（`ConnectionSlot::pendingDisconnect` ＋ `update()`の`drainPendingDisconnects()`）。

**未検証**: Peerテストに該当する呼び出しが無い。`lifecycle_stress`はGATT中`end()`とconnect中`end()`は見ているが、**切断時のqueue purgeとdeferred disconnectは見ていない**。

**なぜ検証が要るか**: どちらも「失敗するはずのものが静かに成功したように見える」経路。purgeが効かなければ切断済み接続のopが生存接続を遅延させ、deferredが効かなければ`disconnect()`がbusy時にfalseを返してアプリが切断できたと誤認する。どちらも症状が出るまで気づけない。

**完了内容**: Peerテスト `gatt_queue_purge` を新設。4件のreadを積んだ直後に`disconnect()`を呼び、queue済み3件が失敗完了（`InvalidState`）として**すべて配送される**こと、電波に出ていた1件は打ち切られず正常完了し**その成功が切断より先に届く**こと（遅延実行の証拠）、`droppedEventCount()`が0であること、その後の再接続・Discoveryが通ることをassertする。積む数はイベントキュー容量（8）を踏まえて4件に抑えた——大きくすると、このテストが観測したいイベント自体が溢れて落ちる。

**検証中に是正が1件出た。** 最初の実行で4件**すべてが成功**してから切断された。原因は `update()` の順序: `pumpGattQueue()` → `drainPendingDisconnects()` で、drainは「そのconnectionのopが実行中なら待つ」だけなので、pumpが毎回次のopを開始して切断が後回しになる。つまりdeferredは「op完了後」ではなく「**キューが空になった後**」で、キューに積み続けるアプリでは無期限に飢餓し、purge経路は`disconnect()`からは到達不能だった（切断時点でキューが空なので落とすものが無い）。

是正: `disconnect()` の時点で当該connectionのqueue済みopを落とす。落とす相手へ送る作業を待つ意味は無く、purge関数自身のコメント（「生存接続の前に詰まらせないため」）とも一致する。実行中のopは触らない。

出典: [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) 小粒2

### B-2. NKROのMTU下限拒否 — ✅ 完了

**実装内容**: 「NKRO keyboard configured かつ `preferredMtu < 32`」を`begin()`が`InvalidArgument`で拒否する（29-byte notifyの無言失敗を防ぐため、silentにMTUを上書きせず明示エラーにする方針）。

**未検証**: `hid_keyboard_nkro`の両sketchは`preferredMtu = 64`を設定しているため、**拒否経路を一度も通っていない**。

**完了内容**: 既存の `hid_keyboard_nkro` に `test_nkro_requires_mtu_32` を追加した。**新しいsuiteを作らなかった理由**: このsuiteのpeer sketchが既にNKROを構成済みで、必要な前提がそのまま揃っている。Peer相手を必要としないテストのために「single」層をharnessへ新設するのは、得られるものに対して変更が大きい。

device sketchが `end()`/`begin()` で境界を歩く: 仕様最小の23、上限の1つ下の31、上限そのものの32。23と31が`InvalidArgument`（detailまで照合）、32が受理されることと、拒否を挟んでもkeyboard構成が残っていることを確認する。`preferredMtu`の既定は247なので、この経路はアプリが明示的に下げたときにしか通らない——だからこそ黙って失敗させず明示エラーにしている。

出典: [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) 小粒3

---

## C. 文書の訂正 — ✅ 完了

「棚卸しで判明した文書の食い違い」C-1〜C-3を反映した。

- C-1 / C-2: DESIGN_DEBTのクラスタBとHID Host auto-rediscoverの状況行を「要実機再確認」から、**どのテストが何をassertしているか**を名指しした記述へ更新。
- C-3: STATUSの残作業から中身が空になっていた項目1を削除し、以降を繰り上げ（日英）。あわせて「残作業の一覧はこの計画が正本」であることをSTATUSの日英両方へ明記した。
- AとBの完了に合わせてDESIGN_DEBT小粒2・3・5の状況行も更新（B-1で見つけた是正の経緯も残した）。

---

## D. リリース作業そのもの

[RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md)が手順の正本。ここには「まだ済んでいないもの」だけを列挙する。

| # | 項目 | 備考 |
|---|---|---|
| D-1 | `--clean`付きで`pytest`（peer + unit）を通し、それを**複数回**繰り返してflaky failure・heap低下・task残留が無いことを確認 | AとBの実装後。今までの実行は差分単位で、全体の`--clean`反復はまだ |
| D-2 | `board-matrix.yml` / `core-matrix.yml`を手動実行し、`docs/BOARDS.<version>.md` / `docs/COMPATIBILITY.<version>.md`を再生成して**対応環境を確定** | 現在の自動実機検証はESP32-S3中心 |
| D-3 | 手動相互運用の実施と記録（実施日・OS/機器version付き） | HID Deviceを外部Host 2種類以上（例: Android・Linux）／HID Hostを市販BLE keyboard 1台以上／Just Worksと静的passkeyを外部実装から／汎用BLE toolでscan・GATT read/write・notify |
| D-4 | メタデータ整合（`library.properties`、`keywords.txt`、`CHANGELOG.md`の`Unreleased`） | **✅ 完了**（versionのbumpはD-6） |
| D-5 | 事前確認チェックリスト一巡（利用者向け文書の日英同期、`API_DESIGN` / `HID_DEVICE_SPEC` / `HID_HOST_SPEC`と公開APIの一致、examples READMEと実装の一致、完了済み計画や古いAPI名へのリンクが残っていないこと） | **✅ 完了**（`docs/memo.ja.md`の扱いのみ未決） |
| D-6 | bump scriptのpreview → release workflow（version・CHANGELOG・release branch・tag・GitHub release）→ 公開後にLibrary Managerからの取得と最小exampleのcompileを確認 | 最後 |

---

## 実行順

**A → B → C → D** を推奨する。

理由は影響の向き。A-1は公開APIが増えるので`keywords.txt`・FEATURE_MATRIX・STATUS・guideに波及し、D-4/D-5より先でなければ二度手間になる。BはAと独立だがどちらもDESIGN_DEBTの状況行を書き換えるため、Cはその後にまとめて一度で済ませる。D-1（`--clean`反復）はコード変更が全部終わってからでないと意味がない。

D-2とD-3はコード変更に依存しないため、AとBと並行して進められる。**D-3の手動相互運用は実機と外部機器の都合で時間がかかりやすい**ので、着手を遅らせない方がよい。

---

## D-4 / D-5 の結果

機械的に照合したので、見つかったものを残す。

### D-4 メタデータ

- **`keywords.txt`に公開APIの取りこぼしが多数あった。** ヘッダから公開型・公開メソッド・`ESP_BLE_*`定数を抽出して突き合わせ、**型10・メソッド60・定数28**を追加した。欠けていたのはリリースチェックリストが名指ししている当のもの——`add*Listener()`族（12件）、`removeGattListener()`、accept list API、report/eventのaccessor（`buttons()` / `usage()` / `capsLock()`など）、`ESP_BLE_HID_*`定数のほぼ全部。`EspBleCallbackList`のメンバと`*Impl`前方宣言、private helperは利用者が名指ししないため除外した。
- **`library.properties`の`paragraph`が出荷内容より狭かった。** 複数同時接続・属性ハンドル指定・標準GATT Service・BLE MIDIが書かれていなかったので実態へ更新した。`name` / `sentence` / `category` / `architectures` / `includes` は一致。
- `CHANGELOG.md`は`Unreleased`に「Initial release」。tagが1つも無く未公開なのでこれが正しい。1.0.0見出しへの移動はD-6。

### D-5 事前確認

- **example READMEに実装に無いAPI名が1件。** `Gap/ScanResponse`と`Gap/ServiceData`のREADMEが`setServiceData()`と書いていたが、実装は`addServiceData()`。修正のついでに`ServiceData`のREADMEへ「同じservice UUIDへ再度addするとブロックが**置き換わる**（2つ目が足されない）ためpayloadは増えない」を追記した——`stop()`→`add`→`start()`を繰り返す例なので、読者が当然抱く疑問である。
- **`HID_DEVICE_SPEC.ja.md`に`hidCustom()`とBoot Protocolが丸ごと無かった。** どちらも出荷済み・example済み・Peer検証済みなのに仕様書のAPI表に存在しない。Custom HIDの行（上限4 report、予約ID 1〜6の回避、Descriptorをライブラリが検証しないこと）、Boot Protocolの既定offとその理由、NKROのMTU下限を`begin()`が**拒否する**こと（「32以上にします」という願望的な記述だった）、Custom ReportのReport Referenceがハンドル指定でしか読めないことを追記。検証節も現在のsuite構成へ更新した。
- **`HID_HOST_SPEC.ja.md`に`setAutoRediscover()`と多listener APIの記述が無かった。** 追記した。
- **`API_DESIGN.ja.md`が「操作は同時1件だけ受理」「次操作を`InvalidState`で拒否」「operation queueは今後の対象」と書いていた**（A-1の作業中に発見・修正済み）。実際は自動FIFOキュー。構成上限も Service 4 / Characteristic 16 → 実際は **8 / 32** だった。
- **`STATUS`が完了済み計画（`PLAN_GUIDE_REVAMP`）へ利用者を送っていた。** DECISIONSの規約（設計判断はDECISIONSへ、過去の計画は残さない）に従い、**「アーキテクチャで確定」節を新設**してラッパ非依存の判断と理由を移し、STATUSの日英からはそこを指すようにした。ラッパ制約とNimBLE制約の切り分け、部分的な自前化が成立しない理由（`ble_gap_adv_start()`のコールバックが全GAPイベントを受け取り、`BLEServer`へ転送する手段が無い）、ビルド構成由来の唯一の真の不可能（EXT_ADV）を記録した。
- 相対リンクは**壊れ0件**、利用者向け文書のja/enは見出し構造が**全ペア一致**（116ペア検査）。

### 未決: `docs/memo.ja.md`

ガイド改訂の発端になったスクラッチ（100行）が残っている。DECISIONS #12 は「旧`memo.ja.md`は移行のうえ**削除済み**」と書いているが、それは別物で、これは後から作られたもの。内容はPhase 0〜11でガイドへ吸収済みで、`PLAN_GUIDE_REVAMP.ja.md`が「発端」としてリンクしている。

削除は元に戻せず、リンクも切れるため独断では実施しない。選択肢は「削除してPLAN側のリンクも外す」「冒頭に『吸収済み・作業なし』と明記して残す」「そのまま」。
