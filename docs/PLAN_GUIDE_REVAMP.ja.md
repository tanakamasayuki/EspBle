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
| 1-2 | Directed Advertising → 0-1では「wrapper経由では不可」として見送ったが、Phase 4b S3のwrapper撤去で実装した（`setDirectedTarget()`） | **完了（Peer検証済み）** |
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
| 4-1 | `addService()` のハンドル化と同一UUID複数インスタンスの許可 | **完了** |
| 4-2 | 同一UUID Characteristic の複数登録許可 | **完了（backend制約により「明示的に拒否」が結論）** |
| 4-3 | `MaxServices` / `MaxCharacteristics` の上限見直し | **完了（4→8 / 16→32）** |

#### 実装したAPI

登録は3段のハンドル連鎖になり、以降の操作とイベントはすべてハンドルで対象を示す。UUID指定のAPIは残していない。

```cpp
const EspBleGattService service = ble.gattServer().addService("181A");
EspBleGattCharacteristic chr = ble.gattServer().addCharacteristic(service, "2A6E", config);
EspBleGattDescriptor desc = ble.gattServer().addDescriptor(chr, "2901");
ble.gattServer().setValue(chr, value);
ble.gattServer().notify(chr, value);
```

`EspBleGattWrite` / `EspBleGattDescriptorWrite` / `EspBleGattSubscription` / `EspBleGattSendResult` に対象ハンドルを追加。UUID文字列はログ用として残している。
内部は UUIDキー → インデックス参照へ変更。`realize()` は同一UUID Serviceへ instance id を自動付与し、attribute handleの必要数もServiceごとに実測値から算出する（従来はbackend既定の15固定）。

#### 実機で判明したbackendの制約

`duplicate_uuid` Peerを作って属性テーブルをダンプしたところ、**同一UUIDの重複は同梱wrapperが2箇所で潰していた**。

1. **同一Service内の同一UUID Characteristicは登録不可**: `BLEService::addCharacteristic()` がNimBLEパスで既存を再利用し、新しい`BLECharacteristic`をGATTに登録せず破棄する（リーク）。`createCharacteristic()` は有効なポインタを返すため、呼び出し側は成功したと誤解する。→ **EspBleは`addCharacteristic()`でこの重複を明示的に拒否する。** 無効ハンドルを返し、送信先のない属性へnotifyし続ける状態を防ぐ
2. **同一UUIDのリモートServiceはClient側で1つに潰れる**: `BLEClient::m_servicesMap` がUUIDキーで、`std::map::insert` が2つ目を黙って捨てる。Server側は両方登録できているが、EspBle Centralからは1つ目しか列挙できない

報告案は [UPSTREAM_REQUEST_ARDUINO_ESP32_NIMBLE_WHITELIST.ja.md](UPSTREAM_REQUEST_ARDUINO_ESP32_NIMBLE_WHITELIST.ja.md) の補遺（問題3・問題4）。

なお**Client側で同一UUID Characteristicをハンドルで撃ち分ける経路は従来どおり動作する**（`m_characteristicMapByHandle`がハンドルキーのため）。HID Reportの撃ち分けは`hid_custom` Peerで検証済み。

#### 移行と検証

- MIDIプロファイルヘルパーと**スケッチ68本**を移行（移行スクリプト＋手作業1本）
- **ビルド207ディレクトリ全PASS**
- Peer実機 **26件PASS**: `gatt_read_write` / `notify_indicate` / `midi_device` / `midi_host` / `persistent_subscribe` / `glucose` / `fitness_machine` / `user_data` / `hid_keyboard_device` / `hid_custom` / `lifecycle_stress`（8件）/ `service_changed` / `battery_service` / `device_information` / `immediate_alert` / `duplicate_uuid`（新規）

**影響範囲が広い**: HID / MIDI を含む全profile helper、`examples/Gatt/**` の全sketch、Peerテスト一式。Phase 1〜3とは独立しているため、別ブランチでの並行作業も可能。

### Phase 4b — wrapper依存の撤去（NimBLEホストAPIへ全面移行）

EspBleのHID Deviceは、同梱wrapperを介さず `ble_gatt_svc_def` / `ble_gatt_chr_def` を自前で組み立て `ble_gatts_add_svcs()` を呼んでいる。だから**同一UUID（0x2A4D）のReport Characteristicを複数公開できている**。
つまりこれまで「backendの制約」と記録してきたものの多くは**wrapperの制約であってNimBLEの制約ではない**。全件を洗い直した結果が次の表。

| # | 制約 | 由来 | 回避 | 手段と規模 |
|---|---|---|---|---|
| 1 | 同一Service内の同一UUID Characteristic（Peripheral） | wrapper `BLEService::addCharacteristic()` が既存を再利用 | ✅ | `ble_gatts_add_svcs()` を自前で。access callback・CCCD・値保持の自前化が必要（**大**、HIDに前例あり） |
| 2 | 同一UUID Serviceが1つに潰れる（Central） | wrapper `BLEClient::m_servicesMap` がUUIDキー | ✅ | `ble_gattc_disc_all_svcs()` / `disc_all_chrs()` を自前で走らせ、handle直指定のread/write/subscribeを追加（**中**） |
| 3 | Directed Advertising 送信不可 | wrapper `start()` が `direct_addr = NULL` 固定 | ✅ | `ble_gap_adv_start()` を直接呼ぶ（**中**） |
| 4 | スキャン側 Filter Accept List | wrapper `BLEScan` が `filter_policy` を非公開 | ✅ | `ble_gap_disc()` を自前で。スキャン結果の配送も自前化（**大**） |
| 5 | Advertisingチャネルマップ | wrapper がNimBLE経路で非公開 | ✅ | #3と同じ自前adv startで同時に解決（**小**、#3の付随） |
| 6 | **Extended / Periodic Advertising** | **ビルド構成** `CONFIG_BT_NIMBLE_EXT_ADV` 無効 | ❌ | **唯一の真の不可能**。プリビルドライブラリに関数自体が存在しない |
| 7 | `connect()` timeout が効かない | wrapper が `esp_ble_gattc_open()` 経由 | ✅ | 放棄方式で対処済み。`ble_gap_connect()` 直呼びなら真のcancelも可能（**中**） |
| 8 | indication確認がcharacteristic単位 | wrapper のセマフォがcharacteristic単位 | ✅ | `ble_gatts_indicate_custom(connHandle, chrHandle)` 直呼びで接続単位に（**小**） |
| 9 | GATT client discoveryのヒープリーク（約2.6 KB/discovery） | wrapper内部の確保 | ✅ | #2の自前discoveryにすればwrapperのremoteオブジェクトを作らせない＝**リーク自体が消える**（#2の付随） |

**#9は重要**: [DESIGN_DEBT.ja.md](DESIGN_DEBT.ja.md) で「対象外（backend由来・修正不能）」としていたヒープリークは、#2を実装すると副次的に解消する。

#### 依存関係（着手して判明）

**#3は#1を前提とする。** NimBLEでは、`ble_gap_adv_start()` に渡したコールバックが、その広告から成立した接続の**すべてのGAPイベント**（切断・購読・MTU・indication確認）を受け取る。自前で広告を開始すると、wrapperの`BLEServer`にはイベントが一切届かず、GATT Serverが機能しなくなる。

- `BLEServer::handleGATTServerEvent(ble_gap_event*, void*)` は **private** で、自前のコールバックから転送できない
- `BLEDevice::setCustomGapHandler()` は存在するが、**NimBLEパスでは一度も呼ばれない**（Bluedroidパスの`gapEventHandler`内でのみ使用）。設定できるだけの死んだAPI

したがって「自前で広告を出す」には「自前でGATT Serverのイベントを処理する」ことがセットで必要になる。Directed Advertisingの用途（bonded peerへの高速再接続）は接続を伴うため、#1なしの#3には実用価値がない。

#### 方針: 個別回避ではなく、wrapperを全面的に外す

当初は上表を1件ずつ回避する計画だったが、**同梱wrapper（`libraries/BLE`）への依存を全部外す**方針に切り替える。判断の根拠は3つ。

1. **半分だけ外すのが最も危険。** wrapperの `BLEServer` / `BLEClient` はadv開始時・connect時に渡したコールバックで当該接続の全GAPイベントを受け取る設計で、片方を自前にすると配線が二重になる。#2の実装中に出た「Notificationがwrite完了イベントを追い越す」バグはその境界で発生した（順序ゲートで修正済み）。全面移行すれば配線は1本になり、この種のバグのクラス自体が消える。過去にクラスタA/Bで実機退行したのも「wrapper machineryの一部だけ自前化」だった
2. **スタックの乗り換えではない。** すでに同じNimBLEホストAPIを呼んでおり、間の薄い層を捨てるだけ。#2で汎用GATT Clientは移行済み、advertising payloadの組み立ても自前、グローバルGAPリスナも登録済み、HID Deviceは元から `ble_gatts_add_svcs()` を自前で呼んでいる
3. **プリビルドライブラリにシンボルが揃っていることを確認した。** `esp32s3-libs/3.3.11/lib/libbt.a` に `nimble_port_init` / `nimble_port_freertos_init` / `ble_svc_gap_init` / `ble_svc_gatt_init` / `ble_store_config_init` / `ble_gap_adv_start` / `ble_gap_disc` / `ble_gap_connect` / `ble_gatts_add_svcs` / `ble_gap_security_initiate` / `ble_sm_inject_io` がすべて存在する（`ble_gap_ext_adv_start` のみ無し＝#6）。NimBLEヘッダもesp32-arduino-libsのグローバルinclude pathにあり、**BLEライブラリへの依存なしでincludeできる**。`CONFIG_BT_NIMBLE_NVS_PERSIST=y` なのでbond永続化も自前で成立する

兄弟ライブラリ [EspBleBluedroid] も同時期に同じ方針転換（ESP-IDFのBLEスタックを直接呼び、BLEクラス依存を外す）を行った。相互接続テストの前提が揃う。

#### 移行の全体像（wrapper API → 置き換え先）

| サブシステム | 現在のwrapper API | 置き換え先 | 規模 | 同時に解決するもの |
|---|---|---|---|---|
| 汎用GATT Client | — | `ble_gattc_*` 直呼び | **完了** | #2・#9 |
| init / deinit | `BLEDevice::init/deinit/getInitialized` | `nimble_port_init()` → `ble_hs_cfg` 設定 → `ble_store_config_init()` → `nimble_port_freertos_init()` → sync待ち | 小 | |
| MTU | `BLEDevice::setMTU()` | `ble_att_set_preferred_mtu()` | 極小 | |
| アドレス / privacy | `setOwnAddr()` / `setOwnAddrType()` / `getAddress()` | `ble_hs_id_set_rnd()` ＋ own_addr_type引数 ＋ `ble_hs_id_copy_addr()` | 小 | |
| Tx Power | `setPower()` / `getPower()` | `esp_ble_tx_power_set()` / `_get()`（**現在もIDF API直呼びで、wrapper非依存**） | 0 | |
| Security | `BLESecurity` / `setSecurityCallbacks()` | `ble_hs_cfg.sm_*` ＋ `BLE_GAP_EVENT_PASSKEY_ACTION` ＋ `ble_sm_inject_io()` | 中 | **SMコールバックのhost task 30秒block、passkey表示の接続attribution推定**（現在DESIGN_DEBTで「修正不能」扱いの2件） |
| Advertising | `BLEAdvertising` / `BLEAdvertisementData` | `ble_gap_adv_set_data()` / `ble_gap_adv_rsp_set_data()` / `ble_gap_adv_start()`（payload生成は既に自前） | 中 | #3・#5 |
| Scan | `BLEScan` / `BLEAdvertisedDevice` | `ble_gap_disc()` ＋ AD構造のパーサ自前 | 中 | #4 |
| Central接続 | `BLEClient::connect()` / `BLEClientCallbacks` | `ble_gap_connect()` ＋ 自前GAPコールバック | 中 | #7。`abandonedClients` / `retireClient` 機構が不要になる |
| GATT Server | `BLEServer` / `BLEService` / `BLECharacteristic` / `BLEDescriptor` | `ble_gatts_count_cfg()` ＋ `ble_gatts_add_svcs()` ＋ access callback ＋ 値保持 | **大**（HID Deviceに前例） | #1。**access callbackが `conn_handle` を受け取るのでDescriptor Write eventの接続ID欠落も解消** |
| HID Host | `BLEClient` / `BLERemoteService` / `BLERemoteCharacteristic` | #2で作った自前スナップショット＋ハンドル指定操作へ寄せる | 中 | discovery経路の一本化 |

#### 移行後も変わらないもの（プリビルドのビルド構成由来）

- **Extended / Periodic Advertising**（#6）: `ble_gap_ext_adv_start` がライブラリに存在しない
- 最大3接続（`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3`）、`MAX_BONDS=3`、`MAX_CCCDS=8`、whitelist 12件

#### 実施順

| 順 | 段階 | 内容 | 状況 |
|---|---|---|---|
| 1 | **S0** | #8 接続単位indication | **完了** |
| 2 | **S1** | #2 汎用GATT Client（discovery・read/write・descriptor・購読・notification受信） | **完了** |
| 3 | **S2** | **GATT Server自前化**（#1）。ここでPeripheral側のGAPイベントを完全に引き取る。S3の前提 | **完了** |
| 4 | **S3** | **Advertising自前化**（#3・#5） | **完了** |
| 5 | **S4** | **Scan自前化**（#4） | **完了** |
| 6 | **S5** | **接続・Security・init/address/MTU自前化**（#7、SMブロッキング解消） | 未着手 |
| 7 | **S6** | HID Hostを自前Client経路へ移行、wrapperの `#include` を全削除、`library.properties`・ドキュメント更新 | 未着手 |

各段階の完了条件は**peerテスト全件PASS**。段階の途中でビルドが通らない期間は許容する（合意済み）。

#### #2 の実施結果（#9はヒープ実測待ち）

**GATT Clientの汎用操作からwrapperを完全に外した。** `BLEClient` は接続の生存確認にしか使っていない。

| 段階 | 実装 |
|---|---|
| discovery | `ble_gattc_disc_all_svcs()` / `disc_all_chrs()` / `disc_all_dscs()` を自前で走らせ、Service・Characteristic・Descriptorを自前のスナップショットへ記録する |
| 対象の解決 | スナップショットをハンドルで、またはUUID対で引く。UUID指定はcallerの表記（`"2a19"` など）をそのまま返し、ハンドル指定のときだけdiscoveryが記録した128bit形を返す |
| read / write | `ble_gattc_read()` / `write_flat()` / `write_no_rsp_flat()` |
| descriptor | 同じ関数を、所属Characteristicの値ハンドルで特定したdescriptorハンドルに対して呼ぶ |
| subscribe | CCCDへの書き込み（Notification `0x0001` / Indication `0x0002` / 解除 `0x0000`） |
| notification受信 | グローバルGAPリスナの `BLE_GAP_EVENT_NOTIFY_RX`。購読テーブルを (接続ハンドル, 値ハンドル) で引くので、UUIDが重複していても取り違えない |

得られたもの:

- **同一UUIDのServiceを複数持つ相手と完全に通信できる**（read / write / subscribe / notify すべて）
- **#9のリーク経路を汎用Clientから外した**。wrapperの `BLEClient::getServices()` は毎回 `clearServices()` してから全discoveryをやり直す作りで、UUIDが重複したServiceの2つ目以降は確保したまま捨てていた。この関数を呼ばなくなったため、汎用Client操作ではwrapper側の確保が一切起きない。ただし[upstream報告案](UPSTREAM_REQUEST_ARDUINO_ESP32_GATTC_DISCOVERY_LEAK.ja.md)は原因を「`BLEClient`が所有するC++オブジェクトの外側」と切り分けており、**自前discoveryでも同じホスト内部確保を踏む可能性が残る。ヒープ実測で確認するまで解消とは書かない**（HID Host / MIDI Hostは自前経路のため未変更）
- Notificationの配送経路が1本になった（wrapperのcharacteristic callbackを経由しない）

副作用として決めたこと:

- **再接続時の購読自動復元は、UUIDが一意なCharacteristicに限る。** 復元はpeerアドレスとUUIDで引くため、同じUUIDが複数あるとどれを購読していたか言えない。該当時は記録しない
- 購読テーブルは8件（`ClientSubscriptionCapacity`）。溢れたら `ResourceExhausted` で失敗させる

実機確認（2ボード）: `peer/duplicate_uuid` を拡張し、同一UUIDの2つのCharacteristicへ**ハンドル指定でread・subscribeし、それぞれのNotificationを取り違えずに受信**するところまで検証した。HID Host / MIDI Host は自前のdiscovery経路を持つため、この変更の対象外。

#### S2 の実施結果

**汎用GATT Serverの属性テーブルを自前で組むようにした。** `BLEServer` / `BLEService` / `BLECharacteristic` / `BLEDescriptor` は汎用サーバから外れた。

| 項目 | 実装 |
|---|---|
| 登録 | `ble_gatts_count_cfg()` ＋ `ble_gatts_add_svcs()`。`ble_gatt_svc_def` / `chr_def` / `dsc_def` の表はサーバ実装が保持する（ホストがポインタを持ち続けるため） |
| 対象の識別 | ホストがaccess callbackへ渡す `ctxt->chr` / `ctxt->dsc` のポインタ同一性。UUIDでは区別できない重複も、これなら一意 |
| 値 | 読み出しは保持している値を `os_mbuf_append()`。書き込みは値を差し替えてイベントを積む |
| CCCD | ホストが自動で付与・管理する（`BLE_GATT_CHR_F_NOTIFY` / `_INDICATE`）。アプリが 0x2902 を自分で `addDescriptor()` するのは**明示的に拒否**する（ホストの管理を二重化してしまうため） |
| 購読状態 | グローバルGAPリスナの `BLE_GAP_EVENT_SUBSCRIBE`。(接続ハンドル, 値ハンドル) で12件まで保持し、切断で破棄 |
| 送信 | notify/indicateはどちらも `ble_gatts_notify_custom()` / `ble_gatts_indicate_custom()`。broadcastは購読中の接続を1件ずつ回る形になった |

得られたもの:

- **#1が解決した。同一Service内に同一UUIDのCharacteristicを複数置ける**。`addCharacteristic()` の拒否を撤去した
- **Descriptor Write eventが接続IDを持つようになった**（access callbackが `conn_handle` を受け取る）。DESIGN_DEBTで「backendが非公開」としていた項目が消える
- broadcast送信のMTU判定が接続単位になった（wrapperの最小MTU一括判定を通らない）

副作用として決めたこと:

- 購読テーブルは12件（`SubscriptionCapacity`）。溢れたら記録できないので `droppedEventCount()` に計上する（黙って配送されなくなるのを避けるため）
- Descriptorへの書き込みが `maximumLength` を超えたら `BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN` で拒否する（切り詰めない）

実機確認（2ボード）: `peer/duplicate_uuid` を「1つのServiceに同一UUIDのCharacteristic 2つ＋同一UUIDのService 2つ」へ拡張し、**3つすべてをハンドル指定でread・subscribeし、Notificationを取り違えずに受信**するところまで検証した。

`ble_gatts_start()` はまだwrapperの `BLEAdvertising::start()` 経由で呼ばれている。S3で自前の広告開始に移すときに引き取る。

#### S3 の実施状況

`ble_gap_adv_set_data()` / `ble_gap_adv_rsp_set_data()` / `ble_gap_adv_start()` を直接呼び、payloadのAD構造も自前で組む。`BLEAdvertising` / `BLEAdvertisementData` は使わない。新しく `setDirectedTarget()`（#3）と `setChannelMap()`（#5）を追加した。

**この段階でPeripheral側のGAPイベントの持ち主が変わる**。広告開始時に渡したコールバックがその接続の全イベントを受け取るため、wrapperの `BLEServer` にはもう何も届かない。そこで一緒に引き取ったもの:

- 接続の成立・切断（接続スロットの登録と解放）
- `ble_gatts_start()`（従来はwrapperの広告開始が兼ねていた。最初の広告開始時に1回だけ実行する）
- `BLE_GAP_EVENT_ENC_CHANGE` によるセキュリティ状態の更新
- `BLE_GAP_EVENT_PASSKEY_ACTION`（表示・入力・数値比較）と `ble_sm_inject_io()`
- `pairOnConnect` の接続時ペアリング開始（`ble_gap_security_initiate()`）

判明した挙動: 非接続広告のPDU種別は **`disc_mode` で決まる**。`BLE_GAP_DISC_MODE_GEN` だとscannable（ADV_SCAN_IND）になり、scan responseを持たないビーコンでも走査要求を受けてしまう。scan responseの中身が無いときは `BLE_GAP_DISC_MODE_NON` を選ぶ。

#### S4 の実施状況

`ble_gap_disc()` / `ble_gap_disc_cancel()` / `ble_gap_disc_active()` を直接呼び、AD構造のパーサも自前で持つ。`BLEScan` / `BLEAdvertisedDevice` は使わない（`#include <BLEScan.h>` を削除）。新しく `EspBleScanConfig::acceptListOnly`（#4）を追加した。

パーサは `parseAdvertisingReport()` の1関数で、AD type 0x02〜0x09・0x0a・0x16・0x19・0x20・0x21・0xff を読む。builderが書く「complete list」だけでなく「incomplete list」も受ける（値は同じで、全部を列挙したと宣言したかどうかしか違わない）。UUIDのon-air形式（2/4/16バイトのリトルエンディアン）から `EspBleUuidValue` を作る `espBleUuidFromLittleEndian()` を `EspBleUuid.h` へ追加し、host unit testも足した。

**アドバタイズとスキャン応答は2つのレポートで届く**ので、scannableな相手はスキャン応答が来るまで保留して1件のScanResultにまとめる。保留表は8件で、溢れたら最も古いものをその時点の内容で報告する（黙って捨てない）。スキャンが終わるとき（`BLE_GAP_EVENT_DISC_COMPLETE` と `stop()`）に、応答が来なかった保留分を吐き出す。passive scanと非scannableな広告は、続きが来ないのでその場で報告する。

実機確認（2ボード）: スキャンに依存する `advertise_scan` / `advertise_payload` / `service_data` / `beacon` / `ibeacon` / `scan_response` / `accept_list` / `address_privacy` / `local_identity` / `connect_disconnect` が通ることを確認した。`accept_list` には**スキャン側**のaccept list（#4）のテストを追加し、accept listが空なら1件も報告されず、相手のアドレスを入れると報告されるところまで検証した。

判明した挙動その2: **`ble_gap_event_listener_register()` のグローバルリスナは `BLE_GAP_EVENT_PASSKEY_ACTION` を受け取らない**。`ENC_CHANGE` や `MTU` は届くのでリスナ側だけで足りると考えていたが、passkeyの表示・入力・数値比較はその接続のコールバック（＝自前広告に渡した `advertisingGapEvent`）にしか来ない。実機のダンプで、リスナ側に一度も届かないこと、接続コールバック側には `action=3`（DISP）が届くことを確認して切り分けた。

実機確認（2ボード）: 新設した `peer/directed_advertising` で、(1) 無向広告で互いのアドレスを学習 →(2) そのCentral宛の有向広告へ切り替えてアドレス指定で接続 →(3) チャネル39のみに絞った無向広告で再び発見・接続、まで通した。有向広告はペイロードを載せられないため、Centralはスキャンではなくアドレス指定で接続する。

副産物: passkey表示 / Numeric Comparisonの**接続attributionが推定でなくなった**（Peripheral接続のみ）。`BLE_GAP_EVENT_PASSKEY_ACTION` が `conn_handle` を持つため、該当スロットを直接引ける。DESIGN_DEBTの「対象外（backend由来）」から1件消えた。

判明した挙動その3: **Centralの `ENC_CHANGE` をグローバルリスナで拾うと、セキュリティ確立イベントが二重に上がる**。wrapperのセキュリティコールバックが同じことを報告するためで、アプリがそこでdiscoveryを走らせていると2回走り、HID Hostがキャッシュしていた `BLERemoteCharacteristic` が入れ替わってLED（Output Report）書き込みが失敗した。リスナ側の `ENC_CHANGE` はPeripheral接続に限定し、Central接続はwrapperの経路に任せる。Peripheral側を自前化したときは「引き取り漏れ」だけでなく「二重計上」も出る、という形の失敗。

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
