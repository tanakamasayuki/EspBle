# 1.0.0リリース前タスクリスト

1.0.0リリースまでの作業リスト。完了した項目は結果を1行残す。

**現在の状態**: Peerテスト65 suite / 82 test、unit 7、example compile 91。src配下に`TODO`/`FIXME`は無い。

## 棚卸しで判明した文書の食い違い

リスト作成のために設計負債の記録と[STATUS.ja.md](STATUS.ja.md)を実テストと突き合わせた結果、**3件は作業ではなく記述の訂正**だった。再検証や再実装をしないよう先に分離しておく。

| # | 記述 | 実際 |
|---|---|---|
| C-1 | 設計負債の記録でクラスタB（Server送信の接続scope化）が「要実機再確認」 | **検証済み。** `notify_indicate` がbroadcast時の`connectionId=0`、queueされた連続3件、接続指定notify（`id=1`）、indicateをすべてassertしている |
| C-2 | 設計負債の記録で「HID Host の再接続時auto-rediscover」が「要実機再確認」 | **検証済み。** `hid_keyboard_host` が、2回目のsecurity確立で**sketchが`discover()`を呼ばない**（`HOST_RECONNECT_SECURITY`）にもかかわらずdiscoveryが走ることをassertしている |
| C-3 | STATUS「1.0.0前の残作業」項目1 = 「FEATURE_MATRIXのHID拡張のうち1.0.0に含めるものを実装する」 | **対象が空。** 未実装のHID拡張は残っていない。今後の候補は[DECISIONS.ja.md](DECISIONS.ja.md)の「優先順位候補」にある2件だが、採用決定ではない |

---

## A. 実装が必要（設計上の穴）

### A-1. handle指定の `readDescriptor()` / `writeDescriptor()` を追加する — ✅ 完了

**当時の状況**: characteristic側には同一UUIDの重複に届くようhandle overload（`readCharacteristic(id, handle)` / `writeCharacteristic` / `subscribe` / `unsubscribe`）があるのに、descriptor側は`readDescriptor(id, serviceUuid, characteristicUuid, descriptorUuid)`のUUID指定しかない。

**なぜ穴か**: 同一UUIDのcharacteristicが並ぶとき、そのどれのdescriptorなのかをUUIDの組では指定できない。**HIDのReport Reference（0x2908）はまさにその状況**——Report characteristicは全部0x2A4Dなので、アプリからは「このReportのReport Referenceを読む」ができない。handle overloadを用意した動機そのものがdescriptor側で満たされていない。ライブラリ内部（HID HostのDiscovery）は`espble_raw`でhandle直指定して読んでおり、公開APIだけが届いていない。

**完了内容**: `readDescriptor(connectionId, descriptorHandle)` と `writeDescriptor(connectionId, descriptorHandle, ...)`（pointer+length版・String版）を追加。`EspBleGattResult` に `descriptorHandle` を追加した（`handle` はそれを持つcharacteristic）。

実装の要点は**解決の順序**。ハンドル指定のdescriptor操作は、discovery snapshotから**descriptorを先に**引き、その `characteristicHandle` で持ち主のcharacteristicを解決する。逆順（UUIDでcharacteristicを引いてからdescriptorを探す）では、重複したcharacteristicのどれかを選べないので成立しない。UUID指定側も結果に解決済みの `descriptorHandle` を載せる。失敗時は要求ハンドルをそのまま返す——どの呼び出しの結果か分からなければ、非同期完了の意味がない。

`hid_custom` Peerを**HIDが本来宣言している方法**へ置き換えた: 各Report Reference（0x2908）をハンドル指定で読み、type byte（1=Input / 2=Output / 3=Feature）で役割を決める。従来のflagsによる代用は捨てず、「宣言されたtypeとflagsが一致すること」の照合として残した（Featureが応答付き書き込みのみであることの確認になる）。ゼロハンドルの拒否と存在しないハンドルの`NotFound`も検証済み。

文書は FEATURE_MATRIX・STATUS・GUIDE_BLE_BASICS（いずれも日英）とTEST_PLAN項目51を更新。`keywords.txt`は`readDescriptor`/`writeDescriptor`が既に登録済みでoverloadは同名のため変更不要（struct fieldは元から列挙していない）。

出典: 設計負債の記録（小粒5、Phase 10の作業中に発見）

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

出典: 設計負債の記録（小粒2）

### B-2. NKROのMTU下限拒否 — ✅ 完了

**実装内容**: 「NKRO keyboard configured かつ `preferredMtu < 32`」を`begin()`が`InvalidArgument`で拒否する（29-byte notifyの無言失敗を防ぐため、silentにMTUを上書きせず明示エラーにする方針）。

**未検証**: `hid_keyboard_nkro`の両sketchは`preferredMtu = 64`を設定しているため、**拒否経路を一度も通っていない**。

**完了内容**: 既存の `hid_keyboard_nkro` に `test_nkro_requires_mtu_32` を追加した。**新しいsuiteを作らなかった理由**: このsuiteのpeer sketchが既にNKROを構成済みで、必要な前提がそのまま揃っている。Peer相手を必要としないテストのために「single」層をharnessへ新設するのは、得られるものに対して変更が大きい。

device sketchが `end()`/`begin()` で境界を歩く: 仕様最小の23、上限の1つ下の31、上限そのものの32。23と31が`InvalidArgument`（detailまで照合）、32が受理されることと、拒否を挟んでもkeyboard構成が残っていることを確認する。`preferredMtu`の既定は247なので、この経路はアプリが明示的に下げたときにしか通らない——だからこそ黙って失敗させず明示エラーにしている。

出典: 設計負債の記録（小粒3）

---

## C. 文書の訂正 — ✅ 完了

「棚卸しで判明した文書の食い違い」C-1〜C-3を反映した。

- C-1 / C-2: 「要実機再確認」のままだった2件を、**どのテストが何をassertしているか**で確認して解消。
- C-3: STATUSの残作業から中身が空になっていた項目1を削除し、以降を繰り上げ（日英）。あわせて「残作業の一覧はこの計画が正本」であることをSTATUSの日英両方へ明記した。
- 設計負債の記録は全項目が完了したため、残すべき判断を[DECISIONS.ja.md](DECISIONS.ja.md)へ移して文書を削除した。

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

理由は影響の向き。A-1は公開APIが増えるので`keywords.txt`・FEATURE_MATRIX・STATUS・guideに波及し、D-4/D-5より先でなければ二度手間になる。Cはコード変更が文書へ及ぶため、AとBの後にまとめて一度で済ませる。D-1（`--clean`反復）はコード変更が全部終わってからでないと意味がない。

D-2とD-3はコード変更に依存しないため、AとBと並行して進められる。**D-3の手動相互運用は実機と外部機器の都合で時間がかかりやすい**ので、着手を遅らせない方がよい。

---

## D-4 / D-5 で見つけて直したもの

| 対象 | 内容 |
|---|---|
| `keywords.txt` | 公開APIの欠落。ヘッダと突き合わせて**型10・メソッド60・定数28**を追加。欠けていたのはチェックリストが名指ししている当のもの（`add*Listener()`族、accept list API、report/eventのaccessor、`ESP_BLE_HID_*`定数のほぼ全部） |
| `library.properties` | `paragraph`が出荷内容より狭かった（複数同時接続・属性ハンドル指定・標準GATT Service・BLE MIDIが未記載）。他フィールドは一致 |
| `CHANGELOG.md` | tagが1つも無く未公開なので`Unreleased: Initial release`が正。1.0.0見出しへの移動はD-6 |
| `Gap/ScanResponse` / `Gap/ServiceData` README | 実装に無い`setServiceData()`を記載していた → `addServiceData()`。同一UUIDへの再addは**置き換え**でpayloadが増えないことも追記 |
| `HID_DEVICE_SPEC.ja.md` | `hidCustom()`とBoot Protocolが丸ごと無かった。NKROのMTU下限は「32以上にします」ではなく`begin()`が**拒否する** |
| `HID_HOST_SPEC.ja.md` | `setAutoRediscover()`と多listener APIが無かった |
| `API_DESIGN.ja.md` | 「同時1件だけ受理」「`InvalidState`で拒否」「queueは今後」が実態と逆。構成上限もService 4/Characteristic 16 → **8/32** |
| `DECISIONS.ja.md` / `STATUS` | ラッパ非依存の判断が完了済み計画の中にしか無く、STATUSがそこへリンクしていた。「アーキテクチャで確定」節を新設して移し、STATUSはそこを指す |

検証: 相対リンク壊れ0件、利用者向け文書のja/enは116ペアすべて見出し構造一致。
