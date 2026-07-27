# BLE通信ガイド拡充計画

[GUIDE_BLE_BASICS.ja.md](GUIDE_BLE_BASICS.ja.md) を「BLE通信を理解するための資料」へ拡充するための計画です。発端は[memo.ja.md](memo.ja.md)。ドキュメント作業だけでは埋まらない穴（exampleの不足・ライブラリAPIの設計不備）が同時に見つかったため、それらを含めた実行順序として整理します。

進捗は各項目の**状況**欄で追跡します（未着手 / 進行中 / 完了）。確定した設計判断は[DECISIONS.ja.md](DECISIONS.ja.md)へ、ライブラリ側の是正は[DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md)へ移送します。

---

## 決定済みの方針

### A. ドキュメントの役割分担

- **ガイドは概念に専念する。** コードの説明はexamples側（sketchコメント＋README）に置く。examplesは現状より説明を充実させる。
- **基礎用語はガイドへ集約する。** 現在 [examples/README.ja.md](../examples/README.ja.md) の「BLEとは / GAP / GATT」節に分散している解説をガイドへ寄せ、各所からガイドへリンクする（逆向きの「詳細は別文書」リンクはやめる）。
- **Central視点とPeripheral視点は並記する。** 量が増えて読みにくくなった場合にのみ章を分ける。
- ガイド内から個別exampleへ飛ばすのは可（実際の使い方はexamplesの責務）。概念の説明を別文書へ飛ばすのは不可。

### B. 不足しているexampleは追加する

APIは実装済みなのにexampleが無い、またはexampleが機能不足なものを埋める。

### C. ライブラリ側は破壊的変更をしてでも正しい姿にする

1.0.0前のため公開APIの破壊的変更は許容する（[DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md)と同じ前提）。

---

## 調査で判明した現状（2026-07-28時点）

| 項目 | 実態 |
|---|---|
| ガイドの視点 | Central=Client のみ。Peripheral側は「Server例は Gatt/Basics/Server 参照」の1行リンクだけ |
| 用語解説 | GAP/GATTの定義が [examples/README.ja.md](../examples/README.ja.md) 側にあり、ガイドはそこへ飛ばしている |
| UUID解説 | ガイド後半（172行目以降）は自己完結しており、流用可能 |
| examplesの構成 | 全ディレクトリが `README.ja.md` / `README.md` を持ち「必要なもの / 動作 / 主なAPI / 期待されるSerial出力」の定型。コード説明をexamples側に置く方針と整合 |
| Service Data advertising | API（`EspBleAdvertising::setServiceData()` / `EspBleScanResult::serviceData`）とPeerテスト `service_data` はあるが、**exampleが無い** |
| ScanDump | `EspBleScanResult` の serviceData / serviceDataUuid を出力していない。`EspBleIBeacon.h` のデコーダも未使用 |
| RPA | `EspBleConfig::ownAddressType = ResolvablePrivate` で対応済み。ただし [Gap/PrivateAddress](../examples/Gap/PrivateAddress/) は `RandomStatic` のみ実演 |
| Directed Advertising | **未対応**（`EspBleAdvertising` にpeer指定なし） |
| Scan Response | Central側は `EspBleScanConfig::active` で要求。Peripheral側は `setScanResponseEnabled()` はあるが、**載るのはnameのみ固定**（[EspBle.cpp:3552](../src/EspBle.cpp#L3552)） |
| Peripheral側の接続拒否 | APIなし。BLE仕様上も「接続要求の承認」機構は無く、現実的手段は ①Filter Accept List（未公開）②接続後に判定して `disconnect()`（現状可能）③属性へ暗号化/認証必須を宣言 |
| Advertisingの未紹介オプション | `setAppearance()`・`setScanResponseEnabled()` はexampleなし。`setInterval()`・`setConnectable()` は Beacon exampleのみ |
| MTU | `EspBleConfig::preferredMtu = 23`。NKROは `begin()` で32未満を明示エラーにする |
| 同一UUIDの複数登録 | `addService()` は同UUIDなら**黙って既存を返す**（[EspBle.cpp:3763](../src/EspBle.cpp#L3763)）。`addCharacteristic()` は (service, characteristic) 重複を InvalidArgument で拒否（[EspBle.cpp:3833](../src/EspBle.cpp#L3833)）。上限は Service 4 / Characteristic 16 |
| Client側の重複UUID | ハンドルベースAPI（`discoveredCharacteristic().handle` → handle指定の read/write/subscribe）で扱える。**Server側と非対称** |

---

## 実行順序

原則は **「API決定 → example → ドキュメント」** の一方向。GAPを完結させてからGATTへ進む。
Phase 4（GATT Server API再設計）を Phase 5/6 の前に置くのが要点で、前倒しするとGAP側の成果が出るまで長く、後ろすぎるとドキュメントを二度書くことになる。

### Phase 0 — 決定と調査（コード変更なし）

| # | 種別 | 項目 | 状況 |
|---|---|---|---|
| 0-1 | 調査 | Directed Advertising が同梱NimBLEで可能か。Legacy directedは `CONFIG_BT_NIMBLE_EXT_ADV` に依存しないため可能性がある。`NimBLEAdvertising` の該当API有無を確認 | 未着手 |
| 0-2 | 調査 | Filter Accept List（whitelist）が `NimBLEDevice` 経由で使えるか。Peripheral側の接続拒否として唯一の正攻法 | 未着手 |
| 0-3 | 設計決定 | **同一UUID複数インスタンスのGATT Server API形**。推奨案: `addService()` が不透明ハンドルを返し `addCharacteristic(serviceHandle, uuid, cfg)` を取る形（Client側のハンドルベースAPIと対称になる） | 未着手 |
| 0-4 | 設計決定 | `preferredMtu` のデフォルト値（23 → 247 など）。NKROの32下限チェックとの整合も併せて判断 | 未着手 |
| 0-5 | 設計決定 | ガイドの章立て確定（GAP / GATT の2部構成、各章内でCentral・Peripheral並記） | 未着手 |

### Phase 1 — GAP側のライブラリ変更（Cのうち影響が浅い側）

| # | 項目 | 状況 |
|---|---|---|
| 1-1 | Scan Responseに任意ペイロードを載せるAPI（現状name固定を解消） | 未着手 |
| 1-2 | Directed Advertising（0-1がOKなら） | 未着手 |
| 1-3 | Filter Accept List / Peripheral側の接続拒否手段（0-2がOKなら） | 未着手 |
| 1-4 | `preferredMtu` デフォルト変更（0-4の決定に従う） | 未着手 |

各項目に Peerテスト追加と [FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md) 更新を伴う。

### Phase 2 — GAP examples（B + Phase 1の新API分）

| # | 項目 | 状況 |
|---|---|---|
| 2-1 | `Gap/ServiceData` 新規（API済・example無しの穴） | 未着手 |
| 2-2 | [Info/ScanDump](../examples/Info/ScanDump/) 拡張（serviceData / serviceDataUuid の出力、`EspBleIBeacon.h` によるiBeaconデコード） | 未着手 |
| 2-3 | [Gap/Scan](../examples/Gap/Scan/) の位置づけ整理（最小例として残す vs ScanDumpへ集約） | 未着手 |
| 2-4 | [Gap/PrivateAddress](../examples/Gap/PrivateAddress/) にRPAモードの実演を追加 | 未着手 |
| 2-5 | Appearance / Scan Response / Directed Advertising / 接続拒否 のexample（Advertise拡張か新規かは粒度次第） | 未着手 |

### Phase 3 — ガイドのGAP章を執筆

基礎用語（GAP / GATT / Central / Peripheral / Advertising / Scanning / Connection / アドレス種別・プライバシー）をガイドへ集約する。Central・Peripheral並記。Phase 2のexamplesを参照する。

| # | 項目 | 状況 |
|---|---|---|
| 3-1 | 用語節の新設（GAPとGATTの役割分担を最初に定義） | 未着手 |
| 3-2 | Advertising節（connectable / non-connectable、載せられるデータ、31byte制限、Scan Response） | 未着手 |
| 3-3 | Scanning節（active/passive、スキャン時間の考え方、取りこぼし） | 未着手 |
| 3-4 | Connection節（接続すべきかの判断材料、Peripheral側の受け入れ、接続パラメータ・MTU） | 未着手 |
| 3-5 | プライバシー節（public / random static / RPA） | 未着手 |

### Phase 4 — GATT Server API 再設計（C本体・最大の破壊的変更）

| # | 項目 | 状況 |
|---|---|---|
| 4-1 | `addService()` のハンドル化と同一UUID複数インスタンスの許可 | 未着手 |
| 4-2 | 同一UUID Characteristic の複数登録許可 | 未着手 |
| 4-3 | `MaxServices = 4` / `MaxCharacteristics = 16` の上限見直し | 未着手 |

**影響範囲が広い**: HID / MIDI を含む全profile helper、`examples/Gatt/**` の全sketch、Peerテスト一式。Phase 1〜3とは独立しているため、別ブランチでの並行作業も可能。

### Phase 5 — GATT examples のコード＋README充実

Phase 4後のAPIで、`Gatt/Basics/*` を中心に「概念はガイド、使い方はここ」の粒度へ書き直す。

### Phase 6 — ガイドのGATT章を執筆

Service / Characteristic / Descriptor、UUIDとハンドルの使い分け、Server・Client並記。既存のUUID解説（ガイド172行目以降）は流用する。

### Phase 7 — 用語解説の集約とリンク整備

[examples/README.ja.md](../examples/README.ja.md) の「BLEとは / GAP / GATT / HID / Security」節をガイドへ吸収し、各example READMEからガイドへの導線を張る。ガイドが安定してから最後に実施する。

### Phase 8 — 英語版と周辺文書

| # | 項目 | 状況 |
|---|---|---|
| 8-1 | 全 `README.md`（en）と英語版ガイドの同期 | 未着手 |
| 8-2 | [FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md) 更新（Phase 1・4の結果を反映） | 未着手 |
| 8-3 | [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) 更新 | 未着手 |
| 8-4 | [DECISIONS.ja.md](DECISIONS.ja.md) へ確定判断を移送 | 未着手 |
| 8-5 | [RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md) との整合確認 | 未着手 |

---

## 未決事項

- Phase 0の5項目（0-1〜0-5）はすべて未決。特に 0-3 はPhase 4の形を決めるため、Phase 4着手前に確定が必要。
- Cの各項目は現在 [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) に記載が無い。Phase 0の決定内容を同文書へ「クラスタD」等として追記し、既存の運用（是正計画→実装→DECISIONS移送）に載せるのが望ましい。
- 2-3（Gap/Scan と Info/ScanDump の役割重複）は、最小例を残す方針かどうかで結論が変わる。
