# 1.0.0リリース前タスクリスト

1.0.0リリースまでの作業リスト。完了した項目は結果を1行残す。

**現在の状態**: Peerテスト65 suite / 87 test、unit 7、example compile 91。src配下に`TODO`/`FIXME`は無い。

## 完了済み

| # | 項目 | 結果 |
|---|---|---|
| A-1 | handle指定の `readDescriptor()` / `writeDescriptor()` | 追加。`EspBleGattResult`に`descriptorHandle`を持たせ、`hid_custom` PeerをReport Reference読みへ置き換えた |
| B-1 | 切断時のGATT queue purgeとGATT op中`disconnect()`の遅延実行 | Peerテスト`gatt_queue_purge`を新設。検証中に是正1件（`disconnect()`時点でqueue済みopを落とす。理由は[DECISIONS.ja.md](DECISIONS.ja.md)接続とGATT 5） |
| B-2 | NKROのMTU下限拒否 | `hid_keyboard_nkro`に`test_nkro_requires_mtu_32`を追加。23 / 31を拒否、32を受理 |
| C | 文書の訂正（棚卸しで判明した食い違い3件） | 反映済み。設計負債の記録は全項目完了のためDECISIONSへ移して削除 |
| D-4 | メタデータ整合（`library.properties`、`keywords.txt`、`CHANGELOG.md`） | 完了（versionのbumpはD-6） |
| D-5 | 事前確認チェックリスト一巡 | 完了 |

---

## D. リリース作業そのもの

[RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md)が手順の正本。ここには「まだ済んでいないもの」だけを列挙する。

| # | 項目 | 備考 |
|---|---|---|
| D-1 | `--clean`付きで`pytest`（peer + unit）を通し、それを**複数回**繰り返してflaky failure・heap低下・task残留が無いことを確認 | 今までの実行は差分単位で、全体の`--clean`反復はまだ |
| D-2 | `board-matrix.yml` / `core-matrix.yml`を手動実行し、`docs/BOARDS.<version>.md` / `docs/COMPATIBILITY.<version>.md`を再生成して**対応環境を確定** | 現在の自動実機検証はESP32-S3中心 |
| D-3 | 手動相互運用の実施と記録（実施日・OS/機器version付き） | HID Deviceを外部Host 2種類以上（例: Android・Linux）／HID Hostを市販BLE keyboard 1台以上／Just Worksと静的passkeyを外部実装から／汎用BLE toolでscan・GATT read/write・notify |
| D-6 | bump scriptのpreview → release workflow（version・CHANGELOG・release branch・tag・GitHub release）→ 公開後にLibrary Managerからの取得と最小exampleのcompileを確認 | 最後 |

---

## 実行順

D-2とD-3はコード変更に依存しないため並行して進められる。**D-3の手動相互運用は実機と外部機器の都合で時間がかかりやすい**ので、着手を遅らせない方がよい。D-1（`--clean`反復）はコード変更が全部終わってからでないと意味がない。D-6が最後。
