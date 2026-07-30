# 1.0.0リリース前タスクリスト

[PLAN_GUIDE_REVAMP.ja.md](PLAN_GUIDE_REVAMP.ja.md)（Phase 0〜11）が完了したため、残作業をここへ集約する。この文書はリリースまでの唯一の作業リストとし、完了した項目は削除せず結果を書き足す。

**この時点での状態**: Peerテスト64 suite / 80 test、unit 7、example compile 91、いずれもグリーン。src配下に`TODO`/`FIXME`は無い。

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

### B-1. 切断時のGATT queue purgeと、GATT op中の `disconnect()` の遅延実行

**実装内容**: `removeConnection`が`purgeQueuedGattOpsLocked(connectionId)`で当該接続のqueue済みopをdropする（generic opは失敗`GattResult`を配送して完了contractを維持、queued HidDiscoverはHID Host切断処理に委ねて静かにdrop、実行中opは無干渉）。`disconnect()`はGATT op中に**rejectせずdeferred**（`ConnectionSlot::pendingDisconnect` ＋ `update()`の`drainPendingDisconnects()`）。

**未検証**: Peerテストに該当する呼び出しが無い。`lifecycle_stress`はGATT中`end()`とconnect中`end()`は見ているが、**切断時のqueue purgeとdeferred disconnectは見ていない**。

**なぜ検証が要るか**: どちらも「失敗するはずのものが静かに成功したように見える」経路。purgeが効かなければ切断済み接続のopが生存接続を遅延させ、deferredが効かなければ`disconnect()`がbusy時にfalseを返してアプリが切断できたと誤認する。どちらも症状が出るまで気づけない。

**検証案**: 複数opをqueueへ積んだ直後に切断し、(a) その接続のqueue済みopが失敗`GattResult`として**すべて配送される**こと（黙って消えない）、(b) GATT op実行中に`disconnect()`を呼んでも`true`が返り、op完了後に実際に切断されることをassertする。GATT queueは実行中1件＋8件なので、積む数は8件以内にする。

出典: [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) 小粒2

### B-2. NKROのMTU下限拒否

**実装内容**: 「NKRO keyboard configured かつ `preferredMtu < 32`」を`begin()`が`InvalidArgument`で拒否する（29-byte notifyの無言失敗を防ぐため、silentにMTUを上書きせず明示エラーにする方針）。

**未検証**: `hid_keyboard_nkro`の両sketchは`preferredMtu = 64`を設定しているため、**拒否経路を一度も通っていない**。

**検証案**: NKRO有効＋`preferredMtu`既定（23）で`begin()`が`false`＋`INVALID_ARGUMENT`を返し、`preferredMtu = 32`へ上げると成功することをassertする。Peer相手を必要としないが、Peer harnessのDUT側だけで完結させる（現在「single」層は無いため）。

出典: [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) 小粒3

---

## C. 文書の訂正

上の「棚卸しで判明した文書の食い違い」C-1〜C-3を反映する。C-3はSTATUSの日英両方。あわせて、AとBの完了時にDESIGN_DEBTの該当項目の状況行も更新する。

---

## D. リリース作業そのもの

[RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md)が手順の正本。ここには「まだ済んでいないもの」だけを列挙する。

| # | 項目 | 備考 |
|---|---|---|
| D-1 | `--clean`付きで`pytest`（peer + unit）を通し、それを**複数回**繰り返してflaky failure・heap低下・task残留が無いことを確認 | AとBの実装後。今までの実行は差分単位で、全体の`--clean`反復はまだ |
| D-2 | `board-matrix.yml` / `core-matrix.yml`を手動実行し、`docs/BOARDS.<version>.md` / `docs/COMPATIBILITY.<version>.md`を再生成して**対応環境を確定** | 現在の自動実機検証はESP32-S3中心 |
| D-3 | 手動相互運用の実施と記録（実施日・OS/機器version付き） | HID Deviceを外部Host 2種類以上（例: Android・Linux）／HID Hostを市販BLE keyboard 1台以上／Just Worksと静的passkeyを外部実装から／汎用BLE toolでscan・GATT read/write・notify |
| D-4 | メタデータ整合（`library.properties`、`keywords.txt`、`CHANGELOG.md`の`Unreleased`） | keywords.txtはA-1で公開APIが増えるため、その後に |
| D-5 | 事前確認チェックリスト一巡（利用者向け文書の日英同期、`API_DESIGN` / `HID_DEVICE_SPEC` / `HID_HOST_SPEC`と公開APIの一致、examples READMEと実装の一致、完了済み計画や古いAPI名へのリンクが残っていないこと） | |
| D-6 | bump scriptのpreview → release workflow（version・CHANGELOG・release branch・tag・GitHub release）→ 公開後にLibrary Managerからの取得と最小exampleのcompileを確認 | 最後 |

---

## 実行順

**A → B → C → D** を推奨する。

理由は影響の向き。A-1は公開APIが増えるので`keywords.txt`・FEATURE_MATRIX・STATUS・guideに波及し、D-4/D-5より先でなければ二度手間になる。BはAと独立だがどちらもDESIGN_DEBTの状況行を書き換えるため、Cはその後にまとめて一度で済ませる。D-1（`--clean`反復）はコード変更が全部終わってからでないと意味がない。

D-2とD-3はコード変更に依存しないため、AとBと並行して進められる。**D-3の手動相互運用は実機と外部機器の都合で時間がかかりやすい**ので、着手を遅らせない方がよい。
