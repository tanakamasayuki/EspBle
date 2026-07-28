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
| 0-1 | 調査 | Directed Advertising が同梱NimBLEで可能か | **完了（結論: wrapper経由では不可）** |
| 0-2 | 調査 | Filter Accept List（whitelist）が使えるか | **完了（結論: 可能）** |
| 0-3 | 設計決定 | 同一UUID複数インスタンスのGATT Server API形 | **完了（ハンドル返却型）** |
| 0-4 | 設計決定 | `preferredMtu` のデフォルト値 | **完了（247）** |
| 0-5 | 設計決定 | ガイドの章立て | **完了（GAP章 / GATT章の2部構成）** |

#### 0-1 / 0-2 の調査結果

調査対象は Arduino-ESP32 同梱の `libraries/BLE`（NimBLE wrapper）。**ローカル実機環境は3.3.10で、examplesのpinは3.3.11**のため、実装時に3.3.11で再確認する。

| 確認項目 | backend API | 結論 |
|---|---|---|
| Directed Advertising | `BLEAdvertising::setAdvertisementType()` で `conn_mode` は設定できるが、`BLEAdvertising::start()` が `ble_gap_adv_start(ownAddrType, **NULL**, …)` と **direct_addr をNULL固定**で呼ぶ（`BLEAdvertising.cpp:1775`） | **wrapper経由では不可**。`ble_gap_adv_start()` の直呼びなら理論上可能だが、advertising状態とGAPイベント配線をwrapperと二重管理することになる。[DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) のクラスタA/Bで実機退行した「wrapper machinery自前化」と同型のリスク |
| Filter Accept List | `BLEDevice::whiteListAdd()` / `whiteListRemove()` / `onWhiteList()` ＋ `BLEAdvertising::setScanFilter(scanRequestWhitelistOnly, connectWhitelistOnly)`（NimBLEでは `filter_policy` = `BLE_HCI_ADV_FILT_NONE/SCAN/CONN/BOTH`） | **可能**。コントローラ側で接続要求とスキャン要求を許可リストに限定できる。memoの「Peripheral側で接続許可の機能はないの？」への正攻法 |
| Scan Responseの任意ペイロード | `BLEAdvertising::setScanResponseData(BLEAdvertisementData&)`。`BLEAdvertisementData` は name / shortName / appearance / flags / manufacturerData / serviceData / complete・partial services / preferredParams / txPower / 任意addData に対応 | **可能**。EspBleが name のみを載せている（[EspBle.cpp:3552](../src/EspBle.cpp#L3552)）のは自前の制限にすぎない |
| 同一UUIDの複数Service | `BLEServer::createService(uuid, numHandles, **inst_id**)` と `getByUUID(uuid, inst_id)` が instance id を持つ | **可能**。backendは元から多重インスタンス対応 |
| 同一UUIDの複数Characteristic | `BLECharacteristicMap::m_uuidMap` は **BLECharacteristic\* をキー**とするmap（値がUUID文字列）。`getByUUID()` が先頭を返すだけ | **可能**。EspBle側がポインタを保持すれば重複は成立する |
| Advertisingの未公開オプション（追加発見） | `BLEAdvertising::addTxPower()`、`BLEAdvertisementData::setFlags()` / `setShortName()` / `setPreferredParams()` / `setPartialServices()` | いずれも未公開。Phase 1で公開範囲を選定する |

**Phase 4の前提が確認できた**: 同一UUIDの多重登録を阻んでいるのは backend ではなく EspBle 自身のUUIDキー設計であり、是正は純粋に自分たちの側の問題。

#### 0-3 の決定 — GATT Server APIは**ハンドル返却型**

`addService()` が不透明ハンドルを返し、`addCharacteristic()` はそのハンドルを受け取ってCharacteristicハンドルを返す。以降の値操作・送信もハンドル指定に統一する。UUID指定のショートカットは**残さない**（APIの二重化を避け、examplesの書き方を一本化するため）。

```cpp
auto svc = ble.gattServer().addService("181A");
auto chr = ble.gattServer().addCharacteristic(svc, "2A6E", config);
ble.gattServer().setValue(chr, value);
ble.gattServer().notify(chr, value);

auto svc2 = ble.gattServer().addService("181A");  // 同一UUIDの2つ目も自然に書ける
```

採用理由: Client側が既にハンドルベースAPI（`discoveredCharacteristic().handle` → handle指定の read/write/subscribe）を持っており、**Server側だけがUUIDキーという非対称を解消できる**。BLEの仕様上「同じUUIDのServiceやCharacteristicが複数存在しうる」以上、UUIDを主キーにする設計自体が誤り。

影響: `EspBleGattServer` の全メソッド、HID / MIDI を含む全profile helper、`examples/Gatt/**` の全sketch、Peerテスト一式。Phase 4で実施する。

#### 0-4 の決定 — `preferredMtu` の既定値は **247**

notify payload 上限が 20 バイト → 244 バイトになり、初学者が最初にぶつかる壁を除去できる。NKROの「32未満は `begin()` で `InvalidArgument`」チェックも設定なしで満たされる。517（ATT最大）は接続ごとのバッファ確保がRAMを圧迫し、最大3接続との相性が悪いため採らない。

#### 0-5 の決定 — 章立ては **GAP章 / GATT章の2部構成**

各章の中で Peripheral → Central の時系列順に並記する。memoの流れ（アドバタイズ → スキャン → 接続 → GATT）にそのまま対応する。

```
1. BLEとは / GAPとGATTの役割
2. GAP編
   2.1 アドバタイズ（Peripheral）
   2.2 スキャン（Central）
   2.3 接続（両者）
   2.4 アドレスとプライバシー
   2.5 初期化時に決めること（MTU等）
3. GATT編
   3.1 Service / Characteristic / Descriptor
   3.2 Server側の組み立て
   3.3 Client側の探索と読み書き
   3.4 Notify / Indicate
4. UUIDを理解する（既存の解説を流用）
```

Phase 3が2章まで、Phase 6が3章と4章を担当する。

### Phase 1 — GAP側のライブラリ変更（Cのうち影響が浅い側）

| # | 項目 | 状況 |
|---|---|---|
| 1-1 | Scan Responseに任意ペイロードを載せるAPI | **完了（Peer検証済み）** |
| 1-2 | ~~Directed Advertising~~ → 0-1により見送り。FEATURE_MATRIXへ❌として記録済み | **完了** |
| 1-3 | Filter Accept ListによるPeripheral側の接続制限 | **完了（Peer検証済み）** |
| 1-4 | `preferredMtu` 既定値を247へ | **完了（Peer検証済み）** |
| 1-5 | Advertisingの未公開オプション公開 | **一部完了（Tx Powerのみ）** |

#### 実装したAPI

```cpp
// 1-1: advertising payload と scan response payload が同じ builder を共有する
class EspBleAdvertisingData {           // 31byte 1面ぶん
  void clear();
  void setName(const char *name);
  bool addServiceUuid(const char *uuid);
  void setManufacturerData(const uint8_t *data, size_t length);
  bool setServiceData(const char *uuid, const uint8_t *data, size_t length);
  void setAppearance(uint16_t appearance);
  void setTxPowerIncluded(bool included);   // 1-5
  bool isEmpty() const;
};
EspBleAdvertisingData &EspBleAdvertising::data();          // advertising payload
EspBleAdvertisingData &EspBleAdvertising::scanResponse();  // scan response payload

// 1-3: accept list と filter policy
enum class EspBleAdvertisingFilterPolicy { Any, ScanRequestFromAcceptList, ConnectionFromAcceptList, Both };
void EspBleAdvertising::setFilterPolicy(EspBleAdvertisingFilterPolicy policy);
bool EspBle::addToAcceptList(const char *address, EspBleAddressType addressType);
bool EspBle::removeFromAcceptList(const char *address, EspBleAddressType addressType);
void EspBle::clearAcceptList();
size_t EspBle::acceptListCount() const;
bool EspBle::acceptListEntry(size_t index, EspBleBond &entry) const;
```

`EspBleAdvertising` の既存setter（`setName()` 等）は `data()` への転送として残したため、既存sketchは無改修。
device nameの自動scan response配置も従来どおり（scan responseに明示的な中身を入れると、その配置は解除される）。

#### 1-3 実装中に判明した上流バグ

`BLEDevice::whiteListAdd()` 系は **NimBLE backendではリンクできない**（`m_whiteList` が宣言のみで未定義）。
加えて `BLEAddress` を `ble_addr_t` へreinterpret_castしており、両者はフィールド順が逆。
そのためEspBleは自前ミラー＋`ble_gap_wl_set()` 直呼びで実装した。
報告案: [UPSTREAM_REQUEST_ARDUINO_ESP32_NIMBLE_WHITELIST.ja.md](UPSTREAM_REQUEST_ARDUINO_ESP32_NIMBLE_WHITELIST.ja.md)

#### 1-5 の残り（未実施）

`setFlags()` / `setShortName()` / `setPreferredParams()` / `setPartialServices()` は公開していない。
Flagsは自動付与（advertising payloadのみ、scan responseには不可）で足り、他は現実の必要性が薄いという判断。
必要になった時点で `EspBleAdvertisingData` に足せる。

#### テスト

実機（ESP32-S3 × 2、Arduino-ESP32 3.3.11）で実行済み。

| テスト | 内容 | 結果 |
|---|---|---|
| `tests/peer/scan_response` | passive scanではname/manufacturer dataが見えず、active scanでのみ見えることを検証 | **PASS** |
| `tests/peer/accept_list` | 制限policy＋到達不能アドレスのみのaccept listでは接続が成立せず、policyをAnyに戻すと成立することを検証 | **PASS** |

回帰確認（いずれもPASS）: `mtu` / `advertise_payload` / `advertise_scan` / `service_data` / `connect_disconnect` / `gatt_read_write` / `notify_indicate` / `hid_keyboard_nkro` / `beacon` / `ibeacon`。
`tests/peer/mtu` が期待する `previous=23` は交換前の初期MTU（仕様上常に23）であり、既定値変更の影響を受けないことを実機でも確認した。

#### 副産物として見つかった既存の不具合 → 修正済み

`accept_list` テスト作成中に、**`connect()` の timeout 引数が効いていない**ことが判明した
（4000 ms 指定に対し失敗検出まで実測約31秒）。Phase 1の変更とは無関係な既存の問題で、
これまでどのテストも通っていなかった経路。

原因は、同梱wrapperのNimBLE `BLEClient::connect()` がBluedroid互換層の `esp_ble_gattc_open()` を使うため
接続試行がホストの追跡するGAP手続きにならず、`ble_gap_conn_cancel()` が `BLE_HS_EALREADY` で空振りすること。
外部から待ちを打ち切る手段がないため、**cancel前提をやめて「放棄」方式へ変更**した
（失敗を即座に配送＋slot解放、遅れて戻ったworkerの結果は破棄、遅れて成立した接続は切断）。
詳細は [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) の「小粒」項目4。

回帰確認（いずれもPASS）: `connect_disconnect` / `gatt_read_write` / `notify_indicate` /
`lifecycle_stress`（8件、再接続とヒープ） / `persistent_subscribe` / `stack_smoke` / `hid_keyboard_host`。

### Phase 2 — GAP examples（B + Phase 1の新API分）

| # | 項目 | 状況 |
|---|---|---|
| 2-1 | `Gap/ServiceData` 新規（API済・example無しの穴） | **完了（実機確認済み）** |
| 2-2 | [Info/ScanDump](../examples/Info/ScanDump/) 拡張（serviceData / serviceDataUuid の出力、`EspBleIBeacon.h` によるiBeaconデコード） | **完了（実機確認済み）** |
| 2-3 | [Gap/Scan](../examples/Gap/Scan/) の位置づけ整理 | **完了（最小例として維持＋ScanDumpへ相互リンク）** |
| 2-4 | [Gap/PrivateAddress](../examples/Gap/PrivateAddress/) にRPAモードの実演を追加 | **完了** |
| 2-5 | Appearance / Scan Response / 接続拒否 のexample | **完了（`Gap/ScanResponse` ＋ `Gap/AcceptList`）** |

#### 追加・変更したexample

| example | 内容 |
|---|---|
| `Gap/ServiceData`（新規） | Environmental Sensing（0x181A）のService Dataとして温度を放送。Manufacturer Dataとの使い分けを表で説明。5秒ごとに `stop()`→`setServiceData()`→`start()` で値を更新 |
| `Gap/ScanResponse`（新規） | advertising payloadとscan responseの2面に分けて31byte制限を回避。各面のbyte内訳をコメントで明示。appearance / Tx Power もここで扱う |
| `Gap/AcceptList`（新規） | Filter Accept Listで接続相手を制限。「BLEには接続要求を承認するcallbackが無い」ことと、3つの代替手段（accept list / 接続後に切断 / 属性を暗号化）を表で説明 |
| `Info/ScanDump`（拡張） | Service Data（UUID＋長さ＋hex）の出力と、iBeacon payloadのデコード（UUID / major / minor / measured power）を追加 |
| `Gap/PrivateAddress`（拡張） | `USE_RESOLVABLE_PRIVATE_ADDRESS` でRandomStatic / RPAを切り替え。RPAはbonding必須である点を表で明示。接続時にpeerアドレスとbonded状態を表示 |
| `Gap/Scan`（説明追加） | 「最小例」であることを明記し、全フィールドを見たい場合はScanDumpへ誘導 |

`examples/README.ja.md` / `README.md` のGAP表に3件を追加済み。

#### 実機確認

ScanDumpを受信側に置き、2台目で各exampleを動かして確認した。

- `Gap/ServiceData` → `servicedata[0000181a-...][2]=c409`（= 0x09c4 = 25.00℃、little-endian）を受信
- `Gap/IBeacon` → `ibeacon uuid=01020304-0506-0708-090a-0b0c0d0e0f10 major=100 minor=1 power=-59` とデコード
- `Gap/ScanResponse` → active scanで `name="EspBle Scan Response" uuid=5266f727-... manufacturer[5]=ffff010203` がマージされて1件で届く

**この過程で2点わかった**（いずれもexample側を修正済み）。

1. 受信側の `serviceDataUuid` は、送信側が16bit表記で指定していても**128bitフル形**で返る。READMEに注意として追記した。
2. `Gap/ScanResponse` の初版はscan responseにname＋appearance＋manufacturer＋Tx Powerを詰めて**36byteで31byte超過**していた。ライブラリは `start()` を `InvalidArgument` で正しく失敗させ、`lastErrorDetail()` が `name does not fit in the 31-byte scan response payload` と溢れたフィールド名を返した（Phase 1で入れたエラーメッセージが機能した形）。appearance / Tx Power をadvertising payload側へ移して解決。

### Phase 3 — ガイドのGAP章を執筆

基礎用語（GAP / GATT / Central / Peripheral / Advertising / Scanning / Connection / アドレス種別・プライバシー）をガイドへ集約する。Central・Peripheral並記。Phase 2のexamplesを参照する。

| # | 項目 | 状況 |
|---|---|---|
| 3-1 | 用語節の新設（GAPとGATTの役割分担を最初に定義） | **完了** |
| 3-2 | Advertising節（connectable / non-connectable、載せられるデータ、31byte制限、Scan Response） | **完了** |
| 3-3 | Scanning節（active/passive、スキャン時間の考え方、取りこぼし） | **完了** |
| 3-4 | Connection節（接続すべきかの判断材料、Peripheral側の受け入れ、接続パラメータ・MTU） | **完了** |
| 3-5 | プライバシー節（public / random static / RPA） | **完了** |

ガイドを0-5の章立てへ全面的に書き直した（419行）。

- **1章「BLEとは」** — Classicとの違い、GAPとGATTの役割分担、4つの役割が2つの独立した軸であること、`update()` 駆動のイベント配送という大原則
- **2章「GAP編」** — 2.1 アドバタイズ / 2.2 スキャン / 2.3 接続 / 2.4 アドレスとプライバシー / 2.5 初期化時に決めること
- **3章「GATT編」** — 構造・操作・時系列図までの概念のみ。Phase 4のAPI再設計を経てPhase 6で本格化する
- **4章「UUIDを理解する」** — 既存の解説を再構成して流用

#### 執筆中に見つかったAPIの非対称

ガイドに「AppearanceとTx Powerは受信側で観測できない」と書いた際に根拠を確認したところ、**backendは供給しているのにEspBleが受け取っていないだけ**と判明した。
Phase 1で送信側に `setAppearance()` / `setTxPowerIncluded()` を追加した以上、受信側で読めないのは非対称な欠陥のため修正した。

- `EspBleScanResult` に `appearance` / `hasAppearance()`、`txPowerLevel` / `hasTxPowerLevel()` を追加（0 dBmが正当な値のため存在フラグを別に持つ）
- `Info/ScanDump` が両方を表示し、Tx PowerとRSSIの差（経路損失）も出す
- `scan_response` Peerを拡張して自動テスト化。Peripheralが両フィールドをadvertising payloadへ載せ、Passive Scanでも届くこと（nameとManufacturer Dataは届かないこと）、Active Scanでも同値であることを判定する。Tx Powerの値はcontrollerが埋めるため範囲と両scan modeでの一致を見る。実機PASS（`appearance=0x0341 txpower=9`）

**Service Dataの複数ブロック対応**も同じ理由で入れた。当初はRAMを理由に先頭1ブロックのみとしていたが、実測するとScan Result 16件ぶんで増加は約1.1 KB（空Stringはヒープを確保しない）で、ESP32-S3の空き約298 KBに対して無視できる規模だった。

- 送信: `EspBleAdvertising::addServiceData()`（`setServiceData()` から改名）で最大4ブロック。同一UUIDの再指定は差し替え、データ省略は削除
- 受信: `EspBleScanResult::serviceData[]` / `serviceDataCount`、UUIDを値比較して引く `serviceDataFor(uuid, data)`
- 上限4の根拠: advertisement＋scan responseで62バイト、1ブロックは最低5バイト（length＋type＋16bit UUID＋payload 1バイト）

`service_data` Peerを2ブロック送出＋UUID検索の検証へ拡張し、実機でPASS（`SERVICE_DATA_COUNT 2`、`serviceDataFor("181A")` が128bitフル形と一致）。`scan_response` / `advertise_payload` / `beacon` / `ibeacon` も回帰PASS。

#### 図はMermaid

GitHubがMarkdown内の ```mermaid フェンスをネイティブに描画するため、シーケンス図はMermaidで書く。ガイドには2枚ある。

- 2.6 GAP編: アドバタイズ → （Active Scanのみの）Scan Request/Response → 判定 → 接続確立 → パラメータ交渉
- 3.3 GATT編: Discovery → Read → Write → 購読 → Notify / Indicate

3.3は元のASCIIアートを置き換えた。

#### 方針

**コードはexampleへ、概念はガイドへ**を徹底した。ガイド内のコードは `ble.update()` を呼ぶloopの3行だけで、これは大原則の説明に不可欠なため残している。
他文書へのリンクはゼロで、外部リンクはすべてexamplesへ向いている。制限を書く箇所ではすべて理由を併記した（31byte上限とExtended Advertising不可の理由、接続拒否ができない理由、RPA周期を変えられない理由など）。

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

- **Phase 0は完了**（0-1〜0-5すべて決着）。次はPhase 1から着手する。
- Cの各項目は現在 [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) に記載が無い。Phase 0の決定内容を同文書へ「クラスタD」として追記し、既存の運用（是正計画→実装→DECISIONS移送）に載せる。Directed Advertising は「対象外（backend由来）」節へ。
- 2-3（Gap/Scan と Info/ScanDump の役割重複）は、最小例を残す方針かどうかで結論が変わる。Phase 2着手時に判断する。
- 1-5（Advertisingの未公開オプション）は公開範囲が未選定。Tx Power は実用性が高いが、Flags / Short Name / Preferred Params は現実の必要性で取捨する（[MEMORY: scope-by-real-world-use] の方針に従う）。
- 調査は Arduino-ESP32 **3.3.10** のソースで実施。examplesのpinは **3.3.11** のため、Phase 1着手時に該当APIを再確認する。
