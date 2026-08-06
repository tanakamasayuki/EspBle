# 無印ESP32対応 実行計画

## 目的

Arduino-ESP32のプリビルドがBluedroid固定である無印ESP32でも、EspBleの公開APIを
ESP32-S3などと同じ形で使えるようにする。NimBLE hostをライブラリ内へ同梱し、
プリビルドのBLE controller（`libbt.a`のBTDM）へVHCI経由で載せる。

初期対応の対象はArduino-ESP32 3.3.11とする。

## 位置づけと利用者向けの注意

**無印ESP32では兄弟ライブラリ[EspBleBluedroid](https://github.com/tanakamasayuki/EspBleBluedroid)の利用を推奨する。**
理由は次の3点で、EspBle側の対応は「NimBLEでも動かせるようにする特殊対応」として扱う。

1. coreのプリビルドがBluedroidなので、EspBleBluedroidはEspressifが保守するhostをそのまま使う。
   EspBleは自前でhostを同梱するため、hostのセキュリティ修正追随をライブラリ側が負う。
2. Bluetooth Classic（SPP等）と同居できるのはBluedroid側だけ。EspBleのNimBLE hostを載せると
   controllerをBLE専用で起動し、Classic用メモリを解放する。
3. EspBleBluedroidは無印ESP32を常設機材で継続的にpeerテストしている。

そのうえでEspBleを無印ESP32で使う場合、**他のEspBle対応チップと挙動が完全に一致するとは保証しない。**
差はhostではなくcontrollerに由来する。

| 項目 | 無印ESP32 | 他のEspBle対応チップ |
|---|---|---|
| controller世代 | BLE 4.2 | BLE 5.x |
| LE 2M PHY / LE Coded PHY | 使えない | 使える（`updatePhy()`） |
| Extended / Periodic Advertising | 使えない | 使えない（hostビルド構成のため。[STATUS.ja.md](STATUS.ja.md)参照） |
| 同時接続数上限 | 3（`CONFIG_BTDM_CTRL_BLE_MAX_CONN=3`） | 3（hostの`CONFIG_BT_NIMBLE_MAX_CONNECTIONS`） |
| 送信電力・タイミング・スキャン取りこぼし | controller実装差でずれる | — |

hostは他チップと同一スナップショットのNimBLEを使い、設定値も同一に揃えるため、GATT・security・
bonding・MTUのAPI上の意味は一致させる。それでも**controller差でタイミング依存のテストが揺れる
可能性があり、無印ESP32はPeerテストで確認できた範囲のみを対応済みとする。**

EspBleBluedroidとの公開API差はEspBle側では管理せず、
[EspBleBluedroid/docs/BLE_BACKEND_DIFFERENCES.ja.md](https://github.com/tanakamasayuki/EspBleBluedroid/blob/main/docs/BLE_BACKEND_DIFFERENCES.ja.md)を正本とする。

## 技術方針

1. **NimBLE hostをEspBleライブラリ内へ同梱する。** 別ライブラリへ隔離せず、外部
   NimBLE-Arduinoへも依存しない。同梱対象は無印ESP32のときだけ有効化する。
2. **同梱ソースは全ファイルをガードする。** すべての`.c`を
   `#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_NIMBLE_ENABLED)`で囲み、
   core同梱NimBLEがある構成では空のtranslation unitになる。`.a`（precompiled）は採らない。
3. **同梱ヘッダは`src/`直下に置かず`src/nimble_esp32/include/`へ隔離する。** ヘッダ内のincludeは
   すべて`"nimble_esp32/include/…"`形式へ機械的に書き換える。
4. **EspBle本体からのNimBLE参照は`src/EspBleNimbleHost.h`のshim1本に集約する。** `EspBle.cpp`の
   BLEロジックは変更しない。
5. **設定は他ターゲットと同値に固定する。** 利用者による上書きは受け付けず、`#error`で拒否する。
6. **既存ターゲット（S3 / C3 / C6 / H2 / P4 Hosted）の生成物を変えない。**

### ガードとヘッダ隔離が必須である理由（検証済み）

- ヘッダを`src/`直下へ置くと、**esp32s3のビルドがcoreのヘッダではなく同梱ヘッダを使う**。
  coreのNimBLEヘッダは`-iprefix`経由で渡されるため、ライブラリの`-I<lib>/src`に負ける。
  結果は「coreの`libbt.a`とリンクしつつ別スナップショットのヘッダでコンパイルする」無言のABI不一致で、
  症状はS3のFlashサイズが基準と180バイトずれることだけだった。隔離後はS3が
  coreのヘッダを使い、生成物が無改造版と**バイト単位で一致**することを確認済み。
- ガードなしでソースを置くと、他ターゲットでも同梱hostがコンパイルされ、coreの`libbt.a`の
  NimBLEと混在してリンクされ得る。

### ソース同梱と`.a`同梱の比較（実測）

| | ソース＋ガード（採用） | `.a`（precompiled） |
|---|---|---|
| 他ターゲットのクリーンビルド | +1.1秒（S3で15.9→17.0秒） | ±0 |
| 無印ESP32のクリーンビルド | +17秒 | ±0 |
| toolchain / IDF版への固定 | なし（ヘッダ変更はコンパイルエラーとしてCIで露見） | あり（故障は実行時） |
| PlatformIO等 | 動く | `precompiled=true`を解釈しないため不可 |
| repository | `.c` 3.9MB＋ヘッダ1.7MB | blob 3.6MB＋ヘッダ1.7MB（core版ごとに再生成すると履歴が増える） |
| 生成ツール | 不要 | `.a`生成ツールが必要 |
| 障害解析 | source levelでbacktrace・step実行 | 不可 |

他ターゲットへの影響が1.1秒に収まるため、`.a`の唯一の利点は無印ESP32の17秒だけになる。

## 取得元と固定するversion

| 取るもの | 取得元 | 内容 |
|---|---|---|
| NimBLE host本体・porting | **espressif/esp-nimble** @ `685675c0128deafdd201c9eb82e61d227364646c` | `nimble/`（host・transport）、`porting/nimble`（os_mbuf, os_mempool, nimble_port ほか）、`porting/npl/freertos`、`ext/`（tinycrypt） |
| ESP glue | **espressif/esp-idf** @ `v5.5.5` の`components/bt/host/nimble/` | `esp-hci/src/esp_nimble_hci.c`（VHCI transport）、`port/src/esp_nimble_mem.c`、`port/src/nvs_port.c`、`port/include/esp_nimble_cfg.h` |
| 不足ヘッダ | 同 esp-idf v5.5.5 | `bt_common.h`、`bt_osi_mem.h`（esp32-libsのinclude treeに存在しない。`hci_log/bt_hci_log.h`、`ble_log.h`、`esp_compiler.h`はcore側にある） |
| 設定値 | core 3.3.11の`esp32s3-libs/include/bt/host/nimble/port/include/esp_nimble_cfg.h` | 生成物。これを起点にすることで既存ターゲットと同一設定を保証する（target差は`MSYS_1_BLOCK_COUNT`程度） |

`685675c…`はesp-idf v5.5.5の`components/bt/host/nimble/nimble` submoduleが指すcommitで、
**core 3.3.11のS3で動いているhostと同一スナップショット**である。

補足:

- ESP-IDFは無印ESP32でのNimBLE hostを公式にサポートしている（menuconfigで選択できる構成）。
  本対応は「Arduinoのプリビルドが選んでいない構成を自前で持ち込む」ものであり、独自移植ではない。
- upstream esp-nimbleの`porting/nimble/src/nimble_port.c`はcontroller初期化を含む
  （無印ESP32向けの`esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)`分岐もある）。
  したがって`EspBle.cpp`は現状どおり`nimble_port_init()`を呼ぶだけでよく、
  controller設定構造体（`BT_CONTROLLER_INIT_CONFIG_DEFAULT()`のmagic）は利用者のcoreで
  コンパイルされるため常に一致する。
- NimBLE-Arduinoは取り込まない。Arduinoへ載せるための改変ノウハウ（includeパス書き換え方式、
  利用者可視の設定ヘッダの作り）の参照先としてのみ扱う。
- ライセンスはesp-nimble / esp-idfともApache-2.0。`src/nimble_esp32/`へLICENSEとNOTICEを同梱し、
  取得元とcommitをREADMEへ明記する。

## 配置

```
src/
  EspBleNimbleHost.h          # NimBLE参照のshim（同梱 or core同梱を切り替え）
  nimble_esp32/
    VERSIONS                  # esp-nimble sha / esp-idf tag / 対象core版
    LICENSE, NOTICE
    include/                  # 同梱ヘッダ（includeはすべてプレフィックス付きへ書き換え）
    *.c                       # 同梱ソース（全ファイルガード付き）
```

## 実装手順

### Phase 0: vendorツール

- `tools/vendor_nimble_esp32.py`: 取得（sha固定）→不要物除去→includeパス書き換え→全`.c`へガード挿入
  →`VERSIONS`更新まで一括。手作業のパッチを残さない。
- 書き換えは同一ディレクトリ相対include（`"ble.h"`など）を誤ってプレフィックス化しない検証を含める。
  試作時に34箇所の誤爆を踏んだため、変換後に両ターゲットのビルドまで確認するスクリプトにする。

### Phase 1: compile対応

- `EspBle.h`のNimBLE不在時`#error`を、無印ESP32では同梱設定を読む分岐へ差し替える。
- `EspBle.cpp`のNimBLE include群を`EspBleNimbleHost.h`へ集約する（ロジックは変更しない）。
- 無印ESP32とesp32s3の両方で代表examplesをビルドし、**S3の生成物が無改造版とバイト一致**することを確認する。

### Phase 2: lifecycle

- `begin()` / `end()`が同梱host構成でも他ターゲットと同じ順序で動くことを確認する
  （controller初期化はesp-nimble側に任せる）。
- bond storeがNVSへ保存・復元されること（`ble_store_config_init()`経路）を確認する。
- 送信電力APIなど、無印ESP32のcontrollerで意味が変わるものを洗い出し、明示的なunsupported結果にする。

### Phase 3: 実機Peerテスト

下記「試験環境」の構成でPeerテストを回す。無印ESP32はcontroller差の影響を受けるため、
**通ったsuiteだけを対応済みとして記録する。**

### Phase 4: 文書・CI・リリース

- `README` / `STATUS` / `FEATURE_MATRIX`へ、対応と上記「位置づけと利用者向けの注意」を反映する。
- `library.properties`の`paragraph`へ、無印ESP32はEspBleBluedroid推奨である旨を追記する。
- `board-matrix.yml`のesp32が❌から✅へ変わる。ビルド時間は92例×約+17秒（1実行あたり約+25分）。

## 試験環境

無印ESP32はEspBleBluedroid側の常設機材を共用する。

| pytest上の位置 | ボード | profile | 環境変数 |
|---|---|---|---|
| 無印ESP32側（新規） | ESP32 | `esp32_peer_host` | `TEST_SERIAL_PORT_ESP32_PEER_HOST=/dev/ttyUSB0` |
| 2台目の無印ESP32（新規） | ESP32 | `esp32_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_ESP32_PEER_DEVICE=/dev/ttyUSB1` |
| 既存の親側 | ESP32-S3 | `s3_peer_host` | `TEST_SERIAL_PORT_S3_PEER_HOST` |
| 既存の2台目Peer | ESP32-S3 | `s3_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE` |

ポート名はEspBleBluedroid側の`tests/.env`と同一にし、1つの配線を両repositoryで共用する。
EspBleとEspBleBluedroidのテストを同時に実行しても、シリアルポートは排他制御されるため
転送が待たされるだけで、実行自体は問題ない。

確認する組み合わせ:

1. **ESP32（EspBle）× ESP32-S3（EspBle）** — 標準構成。既存のS3同士の期待値と比較でき、
   controller差の影響を切り分けられる。
2. **ESP32（EspBle）× ESP32（EspBle）** — 同梱hostどうし。無印ESP32単独環境の利用者の実態に近い。
3. **ESP32（EspBle）× ESP32（EspBleBluedroid）** — 同一チップでhostだけを入れ替えた相互接続。
   EspBleBluedroid側の`tests/interop`はEspBle側をS3で動かしているため、この組み合わせは新規に増える。
   実装はEspBleBluedroid側のsuiteへ追加する。

## 合格条件

- 無印ESP32で代表examplesがビルドでき、`board-matrix`のesp32列が✅になる。
- 既存ターゲットの生成物が変わらない（S3でバイト一致）。
- ESP32 × S3で`stack_smoke`、`gatt_read_write`、`security_bond`、`hid_keyboard_device`、
  `hid_keyboard_host`、`midi_device`、`mtu`、`connection_parameters`が通る。
- bondがNVSへ永続し、再起動後に復元される。
- 通らなかったsuiteと理由（controller制約 / タイミング）が文書に記録されている。
- 文書に「無印ESP32はEspBleBluedroid推奨、EspBleは特殊対応で挙動差の可能性あり」が明記されている。

## リスク

| リスク | 対処 |
|---|---|
| 同梱ヘッダが他ターゲットのcoreヘッダを覆う | ヘッダ隔離＋プレフィックス書き換え。CIでS3生成物のバイト一致を確認する |
| upstream esp-nimbleがArduinoビルドで不足ヘッダを要求する | `bt_common.h` / `bt_osi_mem.h`は同梱。他に出た場合もvendorツール側で解決し、EspBle本体は触らない |
| 設定の上書きでヘッダと同梱実装が食い違う | 上書きを`#error`で拒否。`CONFIG_BT_NIMBLE_MAX_BONDS`はEspBle本体も参照するため特に固定する |
| controller差でPeerテストが不安定 | 通ったsuiteのみ対応済みとし、落ちたものは制約として記録する |
| hostのセキュリティ修正追随 | pinを`VERSIONS`で管理し、esp-idfのnimble submodule更新に合わせて追随する運用を決める |

## 却下した案

- **別ライブラリへ隔離（`EspBleNimble`等）**: ライブラリ内で完結させる方針に反する。
  加えて`__has_include`による条件includeはArduinoのライブラリ解決では機能しない
  （解決器はinclude失敗を検知して初めてライブラリを追加するため）。
- **外部NimBLE-Arduinoへ依存**: 内部includeパスが相手の内部構造依存で壊れやすく、
  スナップショットも第三者改変が混ざる。
- **`.a`（precompiled）同梱**: 上記比較表のとおり、得られるのは無印ESP32のビルド17秒のみ。
- **全ESP32シリーズを同梱hostへ切り替える**: 設定で得があるのは
  Extended / Periodic Advertising、L2CAP CoC、接続数（S3/C3は`BT_CTRL_BLE_MAX_ACT=6`）であり、
  無印ESP32では接続数がcontrollerで3に固定されるため得がない。既存5ターゲットの全再検証と
  P4 Hostedのtransport差分実装（hostedは`libespressif__esp_hosted.a`の`vhci_drv.c`が
  `ble_transport_ll_init`を提供する別経路）を伴うため、いまは採らない。
  Extended Advertisingが必要になった時点で独立の判断として実施する。

## 2026-08-06 Phase 0〜2の実装・検証結果

Phase 0〜2を実装した。Phase 3（実機Peerテスト）は未実施。

### 生成物

- `tools/vendor_nimble_esp32.py`: esp-nimble（tarball、sha固定）とesp-idf（rawファイル）から取得し、
  ヘッダを`src/nimble_esp32/include/`へ隔離、includeを書き換え、全`.c`へガードを挿入し、
  設定ヘッダと`VERSIONS`を生成する。
- `src/nimble_esp32/`: ヘッダ95、ソース83（すべてガード付き）、書き換えたinclude 539箇所、
  固定した設定値102項目。
- `src/EspBleNimbleHost.h`: NimBLE参照のshim。`EspBle.cpp`は13本のincludeをこの1本へ置き換えただけで、
  BLEロジックは無変更。
- `src/EspBle.h`: NimBLE不在時の`#error`に無印ESP32の例外を追加。

### 確認できたこと

- 無印ESP32で代表examples 8本がビルド・リンク成功（`CompileSmoke` 269,768 B、
  `Gap/Connect` 706,328 B、`Gatt/Basics/NotifyServer` 709,756 B、`Gatt/Basics/Client` 710,284 B、
  `Security/StaticPasskeyServer` 709,032 B、`Hid/KeyboardDevice` 709,368 B、
  `Hid/KeyboardHost` 710,920 B、`Midi/MidiDevice` 713,172 B）。静的RAMは33,532 B。
- **既存5ターゲット（esp32s3 / esp32c3 / esp32c6 / esp32h2 / esp32p4）の生成物が変わらない。**
  `Hid/KeyboardDevice`のバイナリを変更前と比較し、差分は`app_elf_sha256`（0xb0〜0xcf）と
  末尾のイメージSHA-256だけ——どちらもELFのパスとビルド時刻から決まる値で、payloadは完全一致。
- controller初期化が`nimble_port_init()`の中に入っている（ELFに`esp_bt_controller_init` /
  `esp_bt_controller_enable` / `esp_bt_controller_mem_release`、VHCI経路、
  `esp_nimble_hci_init`、`ble_transport_ll_init`、`ble_store_config_init`を確認）。
  upstream esp-nimbleを使ったので`EspBle.cpp`側の追加処理は不要だった。
- 固定した設定がS3のsdkconfigと**102項目すべて一致**し、`MAX_BONDS` / `MAX_CONNECTIONS` /
  `ATT_PREFERRED_MTU`はstatic_assertで、`EXT_ADV`無効はコンパイル時に確認済み。
  `-DCONFIG_BT_NIMBLE_MAX_BONDS=9`のような上書きは`#error`で拒否される。
- ビルド時間（12スレッド、クリーンビルド、`Hid/KeyboardDevice`）: esp32s3は15.0秒→15.2秒
  （ガードで空になる83ファイル分）、無印ESP32は17.5秒。`src/nimble_esp32/`は3.9 MB。

### vendorツールで対処が必要だった点

- upstreamは`nimble/host/include`などをinclude pathに置いてビルドするため、
  `"../src/ble_hs_hci_priv.h"`のようにinclude path経由でprivate headerへ到達するファイルがある
  （`ble_svc_cte.c`など）。Arduinoは`<lib>/src`しか渡さないので、この形は
  vendor後の相対パスへ書き換える処理をツールに入れた。
- `bt_common.h`と`bt_user_config.h`は無印ESP32のプリビルドinclude treeに存在しないため同梱する。

### Phase 3: 実機Peerテストの結果（core 3.3.11、ESP32 × ESP32-S3）

`esp32_peer_host` / `esp32_peer_device` profileを対象suiteへ追加し、pytest経由で実行した。

| suite | ESP32 = 親側(Central) | ESP32 = Peer(Peripheral) |
|---|---|---|
| `gatt_read_write`（2 test） | ✅ | ✅ |
| `mtu` | ✅ | ✅ |
| `connection_parameters` | ✅ | ✅ |
| `security_bond` | ✅ | ✅ |
| `hid_keyboard_host` | ✅ | ❌（下記、ESP32固有ではない） |
| `hid_keyboard_device` | ·（親側はcore同梱ラッパ） | ✅ |
| `midi_device` | ·（親側はcore同梱ラッパ） | ✅ |

つまり**無印ESP32はCentral / Peripheralの両役割で、GATT read/write/discovery、MTU交換、
接続パラメータ更新、pairing・bonding（NVS永続）、HID Device、HID Host、BLE MIDI Deviceが動く。**

`hid_keyboard_host`をESP32 Peerで実行すると、親側は接続まで進むがPeer側の`DEVICE_CONNECTED`が
出ずに失敗する。ただし**同じ失敗が標準のESP32-S3 × ESP32-S3構成でも再現する**ため、
無印ESP32の問題ではない（S3の生成物は変更前とバイト一致）。この環境での既存の失敗として別途調べる。
Peer側は接続後も生存していて`isAdvertising()`が0を返す（=接続済みを認識している）ことは確認した。

`stack_smoke`、`advertise_payload`、`midi_host`の親側などcore同梱`BLE`ラッパを使うsketchは、
無印ESP32ではラッパがBluedroidになるため**原理的に実行できない**（自前のNimBLE hostと同一
controllerを共有できない）。これらのsketchの`#error`（`CONFIG_NIMBLE_ENABLED`必須）はそのまま残す。

### 実機で判明した修正点

無印ESP32では`esp_bt_controller_enable(ESP_BT_MODE_BLE)`が失敗し、`begin()`が
`BACKEND_FAILURE the BLE controller did not start`を返していた
（controllerログは`E BLE_INIT: controller enable failed`）。
Arduino-ESP32のプリビルドは`CONFIG_BTDM_CTRL_MODE_BTDM=y`（dual mode）でビルドされているため
`BT_CONTROLLER_INIT_CONFIG_DEFAULT()`の`mode`がBTDMになり、BLEのみを有効化しようとすると
初期化時のmodeと一致せず失敗する。ESP-IDFでNimBLEを選ぶとKconfigがBLE-onlyへ切り替わるため
upstreamには存在しない問題。**vendorツールのパッチとして**`config_opts.mode = ESP_BT_MODE_BLE`と
`config_opts.ble_max_conn = CONFIG_BT_NIMBLE_MAX_CONNECTIONS`を`nimble_port.c`へ入れて解決した
（パッチは`old`が見つからなければ失敗するので、version bump時に必ず気づく）。

### 未実施・保留

- **残りのPeer suiteをESP32構成へ展開する。** 今回profileを追加したのは上表の7 suiteだけ。
  EspBleが両側のsuiteは同じ2行を`sketch.yaml`へ足すだけで対象にできる。
- **実機作業はpytest経由に限る。** ポートの排他はpytest側で管理されており、
  `arduino-cli upload`や`esptool`を直接使うと**待機せずに失敗**する。実際に手動uploadを
  試みてapp書き込み前に中断し（ボードが一時的に起動不能）、EspBleBluedroid側で実行中の
  pytestを巻き込んだ。EspBleとEspBleBluedroidのpytestを同時に走らせるのは問題ない。
- `README` / `STATUS` / `FEATURE_MATRIX` / `library.properties`への反映（Phase 4）。
  実機で確認できたsuiteが決まるまで「対応済み」とは書かない。

## 参考: 試作で得た実測値（core 3.3.11、`Hid/KeyboardDevice`）

| | Flash | 静的RAM |
|---|---|---|
| 無印ESP32（同梱host） | 653,524 B（既定パーティションの49%） | 38,836 B |
| ESP32-S3（core同梱host、無改造） | 638,552 B | 29,864 B |

無印ESP32の653KBのうち約187KBはプリビルドのBTDM controllerで、controller初期化を含めた
機能する構成での値である。
