# 持ち込みhost × Arduino Coreバージョンのテスト計画

無印ESP32でEspBleが持ち込んでいるhostを、現在の対応版であるArduino-ESP32 3.3.11以外の
Coreでも動かせるのか、動かないのかを実測で確定するための計画です。

## 持ち込みhostは2つあり、壊れ方が違う

無印ESP32では、BLEもClassicもCore同梱のhostを使いません。両方EspBleが持ち込んでいます。
形態が違うので、Coreを変えたときの壊れ方も違います。

| host | 形態 | 版の紐付け | 古いCoreでの想定される壊れ方 |
| --- | --- | --- | --- |
| NimBLE（[../src/nimble_esp32/](../src/nimble_esp32/)） | sourceをvendoring（.c 83 / .h 121） | `VERSIONS`にesp-nimble commit、ESP-IDF v5.5.5、arduino-esp32 3.3.11、config取得元を記録 | sketchと一緒にcompileされるので、CoreのIDF headerやporting layerと食い違えばcompile error。**通ってしまえば実機で壊れる** |
| Bluedroid Classic（[../src/esp32/](../src/esp32/)） | 事前buildしたarchive | ESP-IDF v5.5.5 / GCC 14.2.0のABIに固定（[archive再生成手順](CLASSIC_HOST_BUILD.ja.md)） | ABIが違えばlink error。通ってもstruct layoutの差が実行時に出る |

**危険度が高いのはNimBLE側です。** 既に生成済みの[COMPATIBILITY.1.2.0.md](COMPATIBILITY.1.2.0.md)を見ると、
無印ESP32列はBLEの全項目が3.2.0から✅です。compileは通っているのに、実機で動かしたのは
3.3.11だけです。利用者はあの表を「動く」と読みます。Classicのほうは、link errorで
はっきり落ちる公算が高いぶん、むしろ危険が小さい状態です。

## 答えたい問いを2つの軸に分ける

同じ「古いCoreでも動くか」でも、性質の違う問いが混ざっています。混ぜたままテストすると、
結果がどちらの意味なのか読めなくなります。

| 軸 | 問い | 誰の環境が古いか | 失敗したときの意味 |
| --- | --- | --- | --- |
| A | EspBleを古いCoreでbuild・動作させられるか | ライブラリ利用者 | 利用者は3.3.11へ上げる必要がある |
| B | 3.3.11でbuildしたEspBleが、古いCoreで作られた相手と通信できるか | 通信相手（既存機器） | その世代の機器とは繋がらない |

軸Aは「宣言している対応範囲が正しいか」の検証、軸Bは「実利用でどこまで繋がるか」の測定です。
軸Bの相手は世の中に既にあるので、EspBle側をいくら固定しても消せません。

hostが2つあるので、実際には4象限になります。

| | 軸A（自分側が古い） | 軸B（相手側が古い） |
| --- | --- | --- |
| NimBLE | compileは3.2.0から通る。**実機未検証** | peerがCore同梱BLE wrapper（Bluedroid）のcross-stack suiteで測れる |
| Classic | 未測定。link errorの公算 | peerがCore同梱Classic APIのsuiteで測れる |

## 前提（現在の契約）

- `library.properties`は`precompiled=true` / `architectures=esp32`。Classicは無印ESP32だけです。
- 利用者向け文書は「Classicはarduino-esp32 3.3.11のみ」と宣言しています。BLE側は
  compatibility matrixを参照させています。
- ソースにCoreバージョンの検査はありません（`src/EspBle.h:18`のbackend検査は、
  Core同梱NimBLEの有無を見るもので、versionは見ていません）。

## 対象外

- S3 / C3 / C6 / H2 / P4は、hostがCore同梱です。EspBleはそのAPIを呼ぶだけなので、
  食い違いはcompile errorとして出ます。既存のcompile matrixで足り、実機は3.3.11のみとします。
- 3.1系以前は`tools/version_matrix.py`の`CORE_VERSION_FLOOR`（3.2.0）より下なので、
  明示指定したときだけ測ります。

## 対象バージョン

3.2.0以上。IDF世代は3.3.x系がESP-IDF 5.5系、3.2.x系が5.4系で、持ち込みhostは両方とも
5.5.5に紐付けてあります。総当たりせず、境界を先に決める順序で測ります。

1. 3.3.10（同じ5.5系の1つ前）
2. 3.3.0（5.5系の先頭）
3. 3.2.1（5.4系の最終。BLEのcompile matrixが✅を出している下限側）

結果が3.3.11と割れた地点の前後だけ、あとから埋めます。

## Coreバージョンの切り替え方

sketch.yamlへ恒久的にversion違いのprofileを足す案は採りません。

| 案 | 内容 | 判定 |
| --- | --- | --- |
| 1 | sketch.yamlへ`esp32_peer_host_core_3_2_1`のようなprofileをcommitする | 不採用 |
| 2 | `arduino-cli core install esp32:esp32@X`で入れ替える | fallback |
| 3 | 実行時にsketch.yamlのpinを書き換え、必ず復元する | 採用 |

案1が危険なのは、`tools/version_matrix.py`の`set_platform_version()`が
sketch.yaml内の`platform: esp32:esp32 (…)`を**すべて**書き換えるためです（`re.sub`にcount指定なし）。
別versionをcommitしておくと、matrix実行中に黙って上書きされ、終了時に元へ戻るので差分にも出ません。
その間のbuild結果は「pinしたつもりのversion」ではなくなり、matrixが嘘を記録します。
`compile-examples.yml`のほうは`esp32s3`か`default_profile`という名前でprofileを選ぶので、
version違いのprofileを足しても選ばれず、誰にも検証されないまま残ります。加えて
sketch数 × version数だけprofileが増えます。

案2は`~/.arduino15/packages/esp32/hardware/esp32/`が1 versionしか持てない（現在は3.3.11だけ）
ため、切り替えのたびに数GBのdownloadと通常回帰環境の破壊を伴い、他のtestと並行できません。
案3が成立しないと分かったときのfallbackとしてだけ残します。

案3は`version_matrix.py`で既に実績のある方式です。commitされた内容は3.3.11のまま、
書き換えは実行中だけ存在します。

### 実装

`tests/conftest.py`へ次の2つのoptionを足します。既定値なしで、指定しなければ
sketch.yamlには一切触りません。

- `--core-version=X`: DUT sketchのpinだけXにする（軸A用。peerは3.3.11のまま）
- `--peer-core-version=X`: peer sketchのpinだけXにする（軸B用。DUTは3.3.11のまま）
- 両方渡せばsuite全体が古いCoreになる

DUTとpeerを別に指定できる必要があります。たとえば軸AでDUTを3.2.1にするとき、S3のpeerまで
3.2.1にすると、S3ではCore同梱NimBLEが無くcompileできません（`src/EspBle.h:18`のbackend検査）。

session単位のfixtureで、collectされたsuiteのsketch.yamlだけをsnapshotし、書き換え、
teardownで復元します。事故防止は4段構えにします。

1. 復元は`try` / `finally`で行い、testの失敗では飛ばさない。
2. snapshotはscratch側にも退避し、processごと落ちた場合に手で戻せるようにする。
3. 実行scriptの最後に`git diff --quiet -- '*/sketch.yaml'`を確認する。
4. 書き換え対象はcollectされたsuite配下のsketch.yamlに限定し、`examples/`には触らない。

### 先に確認すること（P0-1）

profileでpinしたversionは、arduino-cliがprofile専用の格納先（`~/.arduino15/internal/`）へ
入れ、`packages/`側の3.3.11を置き換えない、という前提で計画しています。これは未検証です。
実装前に1 versionだけで実測し、`packages/esp32/hardware/esp32/`が3.3.11のままであることを
確認します。置き換わるようなら案3は成立しないので案2へ切り替え、頻度をさらに下げます。

**この確認は通常の回帰と同時に走らせません。** もし置き換わる挙動だった場合、実行中の
buildが巻き添えになります。

## Layer構成

### L0: compile / link gate（実機不要）

core version × compileで、通るか通らないかだけを見ます。

- BLE側: **実施済み**。`core-matrix.yml`の生成物が[COMPATIBILITY.1.2.0.md](COMPATIBILITY.1.2.0.md)で、
  無印ESP32は3.2.0から全項目✅です。ここに追加作業はありません。
- Classic側: 未実施。`tools/version_matrix.py`へClassic代表exampleの集合を足し、
  `--targets esp32`で回します。sketch.yamlの復元機構は既存のものをそのまま使います。
  - 対象example: `Classic/SppServer`、`Classic/SppStream`、`Classic/HidKeyboard`、
    `Classic/A2dpSinkAvrcp`、`Classic/HfpClientRaw`、`Classic/Inquiry`、`Classic/RadioSettings`
- 合格の定義: 3.3.11がPASSであること、かつ他のversionの結果が文書の宣言と一致すること。
  **古いversionのFAILは失敗ではなく期待結果**です。
- 昇格条件: 3.3.11以外でPASSしたversionは、必ずL1へ進めます。ABIが違うのにlinkだけ通る
  状態が、いちばん静かに壊れます。BLE側は既にこの状態です。
- 所要: 1 versionあたりdownloadを含めて15〜25分。実機不要。
- 頻度: releaseごと（workflow_dispatch）。毎commitでは回しません。1 coreあたり数GBのdownloadで、
  結果が変わるのはCoreがreleaseされたときだけです。

### L0の副産物: エラーの読みやすさ

Classicが対応外のCoreで落ちるとき、いまは大量のundefined referenceになる見込みです。
利用者が原因を特定できないので、L0の結果を見たうえで`EspBleClassic.h`へversion guardを
入れるかを決めます。

```c
#if ESP_ARDUINO_VERSION != ESP_ARDUINO_VERSION_VAL(3, 3, 11)
#error "EspBle's Bluetooth Classic host is built for Arduino-ESP32 3.3.11"
#endif
```

置き場所は`EspBleClassic.h`です。`EspBleClassicBuild.h`へ入れると、無印ESP32でBLEだけ
使うsketchまで巻き込みます。BLE側に同じguardを入れるかは、L1の結果が出るまで決めません。
実機で動くと分かった範囲まで狭めるのは、利用者から動く構成を取り上げることになります。

### L1: 軸Aの実機（自分側が古いCore）

ここが今回の中心です。BLE側は「compileが通っているのに実機未検証」という穴が既にあります。

- BLE: 無印ESP32を親、S3をPeerにして代表smokeを実行します。
  `gatt_read_write` / `security_bond` / `hid_keyboard_host` / `mtu` / `connection_parameters`
  （[リリースチェックリスト](RELEASE_CHECKLIST.ja.md)の代表smokeと同じ集合）。
  持ち込みNimBLEはsourceでbuildされるので、controllerとの境界（HCI、controller config、
  NVS、FreeRTOS port）が版差の出どころです。接続、GATT、暗号化、bond再接続まで見れば
  そこは通過します。
- Classic: L0でPASSしたversionだけ。`classic_inquiry` / `classic_pairing` /
  `classic_spp_stream` / `classic_hid_api` / `classic_a2dp_sink_profile`で、起動、
  `begin()`成功、heap、inquiry、SPP echo、profile初期化まで。
- 実行は`--core-version`付き。
- 合格の定義: 3.3.11と同じ結果になること。1項目でも差があれば、そのversionを
  「compileは通るが動作は未保証」として[STATUS](STATUS.ja.md)とcompatibility matrixの
  凡例に明記します。**✅の意味を「buildできる」に限定して書き直す**のが最低限の対応です。
- 所要: BLE代表smokeが15分程度、Classicが20分程度（downloadを除く）。無印ESP32が1〜2台。
- 頻度: L0の結果が変わったとき、およびrelease前の技術検証。

### L2: 軸Bの実機（相手だけ古いCore）

**新しいsketchは不要です。** peer側がEspBleを一切linkしないsuiteが既に7本あります。
DUTは3.3.11のEspBleに固定し、`--peer-core-version`でpeerだけpinします。

| suite | peer側の実装 | 何が古くなるか |
| --- | --- | --- |
| `core_host_gatt` | Core同梱`BLE`ラッパ（Bluedroid） | GATT / discovery / CCCD |
| `core_host_security` | 同上 + `esp_ble_remove_bond_device` | pairing / SC / bonding |
| `core_host_hid` | `BLEHIDDevice` | HID over GATT |
| `core_host_midi` | 手組みのMIDI service | BLE MIDI packet |
| `classic_core_host_spp` | Core同梱`BluetoothSerial` | SPP / RFCOMM |
| `core_host_a2dp` | `esp_a2d_source_*` | A2DP / SBC / AVDTP |
| `core_host_hfp` | `esp_hf_ag_*` | HFP / SLC / AT |

Classic HIDはCoreが`CONFIG_BT_HID_ENABLED`を持たずpeerを作れないため、この軸では扱えません（既知）。

peer sketchの移植性で1点だけ手当てが要ります。`core_host_a2dp`と`core_host_hfp`のpeerが読む
`esp32-hal-alloc-bt-classic-mem.h`は3.3.x系にある前提のheaderなので、次の形にします。
古いCoreはそもそも起動時にClassic BTメモリを解放しないため、無い場合は何もしなくて正しい動作です。

```cpp
#if __has_include(<esp32-hal-alloc-bt-classic-mem.h>)
#include <esp32-hal-alloc-bt-classic-mem.h>
#endif
```

他にも古いCoreで欠けるAPIが出たら、同じく`__has_include`か`ESP_ARDUINO_VERSION`で
peer側だけを分岐させます。**DUT側（EspBle）には分岐を入れません。**

- 合格の定義: 3.3.11 peerと同じ判定がすべて通ること。落ちた項目は「その世代の相手とは
  この機能が繋がらない」という利用者向けの事実として記録します。
- 所要: 1 versionあたり7 suiteで25分程度（downloadを除く）。
- 頻度: manual。Coreのlineupが増えたときと、releaseの技術検証で回します。

## 毎回やるか、manualか

| Layer | 位置づけ | 実行タイミング |
| --- | --- | --- |
| 通常のpeer回帰 | 3.3.11のみ。変更なし | 毎回 |
| L0 compile gate | release gate | releaseごと（CI dispatch） |
| L1 軸A実機 | 条件付き | L0の結果が変わったとき + release前 |
| L2 軸B実機 | manual | Core lineup更新時とrelease技術検証 |

毎回の回帰には入れません。1 versionあたり数GBのdownloadと15分以上を要する一方、
結果が変わるのはCoreが新しくreleaseされたときだけで、毎日回しても同じ表が出ます。
既定の`pytest peer/`が3.3.11だけを見る現状は維持します。

## 実施順

| 段階 | 内容 | 完了条件 |
| --- | --- | --- |
| P0-1 | profile pinの隔離を1 versionで実測（他の回帰と同時に走らせない） | `packages/`側の3.3.11が保持される |
| P0-2 | `--core-version` / `--peer-core-version`と復元機構を実装 | 実行後に`git diff`が空 |
| P1 | **BLEのL1**を3.2.1で実行 | 既にある✅の意味が確定する |
| P2 | ClassicのL0を3.3.10 / 3.3.0 / 3.2.1で実行 | 境界が確定する |
| P3 | P2でPASSしたversionだけClassicのL1 | 実機結果が出る、または対象なしと確定 |
| P4 | L2を代表2 version（3.3.0と3.2.1想定）で実行 | 7 suiteの合否が出る |
| P5 | 文書反映と`#error` guardの採否決定 | 下表のとおり |

P1を先頭に置いているのは、そこが唯一「利用者が既に動くと読める状態で、誰も確かめていない」
組み合わせだからです。Classicは「対応外」と書いてあるぶん、確認が遅れても実害が小さいです。

## 記録先

| 文書 | 何を書くか |
| --- | --- |
| [STATUS](STATUS.ja.md) / [STATUS.md](STATUS.md) | 実機で確認したCore範囲と、繋がらない相手の世代 |
| [README](../README.ja.md) / [README.md](../README.md) | 「3.3.11のみ」の根拠を実測結果へ差し替える |
| `docs/COMPATIBILITY.<version>.md` | Classic節の追加と、✅の意味（buildできる / 実機確認済み）の区別 |
| [テスト計画](../tests/TEST_PLAN.ja.md) | L1 / L2の実行方法と頻度 |
| [リリースチェックリスト](RELEASE_CHECKLIST.ja.md) | L0をreleaseの手順として追加 |

## 実測結果

### P0-1: profile pinは隔離される（2026-08-16）

repo外へ複製したsketchのpinを3.2.1にしてcompileし、導入先を確認しました。

```
Used platform  Version  Path
esp32:esp32    3.2.1    /home/mt/.arduino15/internal/esp32_esp32_3.2.1_db7af308ecb78b10
```

`~/.arduino15/packages/esp32/hardware/esp32/`は3.3.11のままです。pinしたversionは
arduino-cliがprofile専用の格納先へ入れ、通常環境のcoreを置き換えません。一度入れれば
そこに残るので、2回目以降のdownloadも起きません。案3（実行時に書き換えて復元）で進めます。

同時に、Classicの最初の実測が取れました。3.2.1ではlink以前にcompileが止まります。

```
/home/mt/dev/EspBle/src/EspBleClassic.cpp:19:10: fatal error:
  esp32-hal-alloc-bt-classic-mem.h: No such file or directory
```

archiveのABIまで到達する前に、EspBle自身のClassic sourceがCore 3.3.x系のheaderを
要求して落ちています。利用者にはheaderが無いとしか見えないので、`#error`で理由を出す
価値がここで裏付けられました。

### P1: BLEのsuiteも3.2.1では動かせない（2026-08-16）

無印ESP32をDUT、S3をPeerにして代表smokeを`--core-version 3.2.1`で実行したところ、
5 suiteすべてがcompileの時点で失敗しました。原因はP0-1と同一です。

```
src/EspBleClassic.cpp:19:10: fatal error: esp32-hal-alloc-bt-classic-mem.h: No such file or directory
```

BLEしか使わないsketchでも、Arduinoはライブラリの`src/`にある`.cpp`をすべてcompileします。
無印ESP32では`CONFIG_BT_CLASSIC_ENABLED`により`EspBleClassic.cpp`の中身が有効になるので、
**Classicを使うかどうかに関係なく**このheaderが要ります。

そのheaderは[arduino-esp32 #12574](https://github.com/espressif/arduino-esp32/pull/12574)で
**3.3.9に追加**されたものです（3.3.8以前のtagには存在しません）。

したがって現在の`src/`は、無印ESP32ではcore 3.3.9以上を必須にしています。
[COMPATIBILITY.1.2.0.md](COMPATIBILITY.1.2.0.md)のesp32列が3.2.0から✅なのは、
Classicが`src/`へ入る前に生成された表だからで、**現在の実装とは一致していません**。
releaseまでに再生成が必要です。

### L0: 無印ESP32のcompile matrix（2026-08-16）

BLE代表7 example + Classic 9 exampleを、無印ESP32向けに5 versionでcompileしました。
**16 example × 3.2.1 / 3.3.8 / 3.3.9 / 3.3.10 がすべて失敗、3.3.11だけ全passです。**
理由は2つに分かれます。

| core | 同梱ESP-IDF | `esp32-hal-alloc-bt-classic-mem.h` | `ESP_HIDD_APP_DESC_LIST_LEN_MAX` | 結果 |
| --- | --- | --- | --- | --- |
| 3.2.1 | v5.4系 | 無 | 無 | ❌ header |
| 3.3.8 | v5.5系 | 無 | 無 | ❌ header |
| 3.3.9 | v5.5.4 | 有 | 無 | ❌ 定数 |
| 3.3.10 | v5.5.4 | 有 | 無 | ❌ 定数 |
| 3.3.11 | v5.5.5 | 有 | 有（2048） | ✅ |

```
src/EspBleClassicHid.cpp:720:37: error: 'ESP_HIDD_APP_DESC_LIST_LEN_MAX' was not declared in this scope
```

`ESP_HIDD_APP_DESC_LIST_LEN_MAX`はESP-IDF v5.5.5で入った定数で、archiveをbuildしたIDFと
同じ版です。3.3.9 / 3.3.10はv5.5.4なので、headerを通過しても次で止まります。

つまり「Arduino-ESP32 3.3.11のみ」という宣言は正しく、**無印ESP32ではBLEだけを使う場合も
同じ制約**です。当初この計画で疑っていた「compileが通るのに実機未検証」という危険な
状態は、無印ESP32には存在しませんでした。危険なのは公開済みの表のほうです。

### 壁の実体は3つの小さなAPIずれだった（2026-08-16）

「3.3.11のみ」は狭すぎるという指摘を受けて、止まっている箇所を1つずつ潰して測り直しました。
出てきたのはABIの不一致ではなく、Core同梱IDFのheaderにある**名前の増減**だけです。

| 版 | 止まった箇所 | 性質 | 対処 |
| --- | --- | --- | --- |
| 3.3.8以下 | `esp32-hal-alloc-bt-classic-mem.h`が無い | Core 3.3.9で新設されたheader | `__has_include`。無い版はそもそもClassic BTメモリを解放しない |
| 3.3.8以下 | `bleInUse()`が無い | 同じheader由来 | 無い版はdual modeで起動（BLEを使うsketchを壊さない側へ倒す） |
| 3.3.10以下 | `ESP_HIDD_APP_DESC_LIST_LEN_MAX`が無い | ESP-IDF v5.5.5で追加 | 2048を自前定義。SDP padの上限はrecordを持つhost側の値 |
| 3.3.0 | `ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR`が無く`SRN` | v5.5.5での綴り修正（値は同じ0x2） | aliasを定義 |

shimは当時`EspBleClassicCoreCompat.h`へ集約しました（3.3.11ではどちらの名前もCore側に
存在するため`#ifndef`で無効）。**このheaderは案Dの実施で不要になり削除済みです**——契約header
（`src/esp32/include/`）がv5.5.5の名前を常に提供するため、shimの出番自体が消えました。

そのうえでcompile matrixを取り直すと、**3.3.0から3.3.11までbuildもlinkも通ります**。
5.5.5でbuildしたarchiveは、5.5.4以前のIDFを載せたCoreとlinkできています。

実機も確認しました。DUTを3.3.10でbuildしたBLE代表smokeは**4 passed（9:00）**で、
GATT read/write、bonding、MTUが通っています。

Classicは無印ESP32 2台とも3.3.10でbuildし、`classic_inquiry` / `classic_pairing` /
`classic_spp_stream` / `classic_hid_api` / `classic_a2dp_media` / `classic_hfp_client`の
**6 suiteすべてが通りました**。まとめて実行したときは`classic_inquiry`だけが
`INQUIRY_READY`をbuffer空でtimeoutしましたが、単独実行では3:39で通ります。
[起動banner待ち](../tests/TEST_PLAN.ja.md#起動banner待ちを避ける)の取りこぼしで、
3.3.10の退行ではありません（このsuiteは`probe`化がまだ済んでいない側です）。

つまり**3.3.10はbuildできるだけでなく、BLEもClassicも実機で動きます。**

### 版数guardの追加

失敗の理由が利用者に伝わらないので、`EspBleClassicBuild.h`へguardを入れました。
3.3.11未満は`#error`、3.3.11超は`#warning`です。後者を止めないのは、ABIが未検証であって
壊れると分かったわけではないからで、黙って食い違うよりは言うほうがましだという判断です。

### L2: 相手を3.3.10でbuildしても繋がる（2026-08-16）

DUTを3.3.11のEspBle、peerを`--peer-core-version 3.3.10`でbuildして、peerがEspBleをlinkしない
7 suite（`core_host_gatt` / `core_host_security` / `core_host_hid` / `core_host_midi` /
`core_host_a2dp` / `core_host_hfp` / `classic_core_host_spp`）を実行し、**7 passed（18:55）**でした。
GATT、pairing、HID over GATT、BLE MIDI、SPP、A2DP、HFPが版をまたいで噛み合っています。

軸Aの厳しさは通信仕様ではなくbuild時のIDF依存から来ている、という切り分けの根拠になります。

### 機能ごとに下限が違う: HFPのSCO audio（2026-08-16）

3.3.0で`classic_hfp_client`だけが落ちました。単独実行でも再現し、SCOが`state=1` / `codec=0`の
まま確立しません。原因はEspBle側ではなく、**Coreが同梱するprebuilt controllerの設定**でした。

| Core世代 | prebuilt sdkconfig |
| --- | --- |
| 3.3.0〜3.3.6（`esp32-arduino-libs` packaging） | `CONFIG_BT_HFP_AUDIO_DATA_PATH_PCM=y` |
| 3.3.8以降（`esp32-libs` packaging） | `CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=y` + `CONFIG_BT_HFP_WBS_ENABLE=y` |

古いCoreのcontrollerはSCO音声を外部codec chipへ流すPCM pathでbuildされています。EspBleのHFPは
raw SCOをHCI経由で運ぶので、controllerが合意しない限りaudio linkは張れません。

**これはarchiveでもsource持ち込みでも解決しません。** controllerは常にCore側のbinaryだからです。
host側の配布形式を変えても届かない領域がある、という切り分けとして重要です。

ただしこの設定差だけでは全部を説明できません。版ごとに単独実行した結果は次のとおりです。

| Core | HFP client suite | 止まり方 |
| --- | --- | --- |
| 3.3.0 / 3.3.6 | ❌ | SCOが`state=1 codec=0`のまま。controllerがPCM path |
| 3.3.8 | ❌（2回とも再現） | `hfpAudioGateway().begin()`がClient起動後も成功し、HFP役割の排他が効かない |
| 3.3.9 / 3.3.10 | ✅ | — |

3.3.8の件は当初「排他の不発」と読みましたが、調査の結果**誤読**でした。実体は次のとおりです。

1. serialを直接開いてreset直後を取ると、DUTは起動時に
   `HFP_CLIENT_STACK_FAILED failed to start the Classic controller; Classic memory may
   already be released`を1度だけ出して、setupを離脱していました。この行はmonitor接続前に
   流れて失われます（既知の起動banner競合）。
2. sketchの`?`応答は、setupが離脱すると**既定値のまま**`ag=1 error=None`を返し続けます。
   これが「AG beginが通ってしまった」ように見えた正体です。排他は実行すらされていません。
   peer側の「正しく見えた」応答も同じ既定値で、**期待値と偶然一致していただけ**でした。
   両ボードとも同じ理由で死んでいました。
3. 根本原因はCoreのBTメモリ保持機構の**過渡期**です。`btInUse()`は3.3.6以前は無条件`true`、
   3.3.9以降は分割header（`esp32-hal-alloc-bt-classic-mem.h`等）のconstructorで立てる方式で、
   EspBleは両方に対応済みでした。しかし**3.3.7 / 3.3.8だけは単一flag（`_btLibraryInUse`）を
   `esp32-hal-bt-mem.h`で立てる中間方式**で、EspBleはこのheaderを知らず、flagが立たないまま
   `initArduino()`がBTDM全メモリ（BLE込み）を解放していました。

つまりEspBleの排他logicに問題はなく、**3.3.7 / 3.3.8ではHFPどころかBLEもClassicも起動できて
いなかった**ことになります（compile matrixでは見えず、3.3.8の実機はこのsuiteしか回していなかった）。
対処は`esp32-hal-bt-mem.h`を`__has_include`の連鎖へ追加（`EspBle.cpp`と`EspBleClassic.cpp`）。
修正後の3.3.8実機はHFP suiteが**1 passed（3:02）**で、SCO audioの実流通
（`state=2 codec=2 frame=57`）まで確認しました。3.3.8のcontrollerはHCI audio pathなので、
**HFP audioの実機確認済み下限は3.3.9から3.3.8へ下がります**。

3.3.7も測定しました: `esp32-hal-bt-mem.h`を持つ（同じ過渡機構＝同じ修正が効く）、
CompileSmokeのcompile通過、ただしcontrollerは`CONFIG_BT_HFP_AUDIO_DATA_PATH_PCM=y`なので
HFP audioは対象外のままです。

副産物として、test観測の罠を1つ記録します: sketchの`?`応答が既定値と「正常時の値」を区別
できない形だと、起動失敗が別の失敗（今回なら排他）に化けます。状態報告には「stackが起動
したか」を含めるべきで、既存suiteの`probe`化TODOと合わせて見直します。

この実測を受けた配布形式（archive / source / 折衷）の比較と推奨は
[Classic host配布形式の再評価](PLAN_CLASSIC_HOST_DISTRIBUTION.ja.md)が正本です。

### 案D実施後の再測定（2026-08-16）

契約15 headerを`src/esp32/include/`へvendorし（`tools/vendor_classic_contract.py`、
IDF v5.5.5とbyte一致）、`src/`のbluedroid API includeを自前へ張り替えた結果です。

- **3.2.x（IDF 5.4）のcompile失敗は消えました。** 構造体メンバ差はcoreのheaderを読んで
  いたことが原因で、契約が自前になると発生源ごと消えます。
- 残った差は**link時のundefined `esp_log` 1個**でした。archiveの外部import 89 symbolの
  うち、IDF 5.4のcoreに無いのはこれだけです（IDF 5.5で入ったlog v2の入口）。
  `src/EspBleClassicLogCompat.c`（3.3.0未満のcoreでだけ実体を持つ転送関数）で解消。
- compile matrixは**3.2.0 / 3.2.1 / 3.3.0 / 3.3.8 / 3.3.11 × 7 example（BLE + Classic全profile）で
  全pass**。
- 実機: 3.3.11の出荷状態（repinなし）でClassic 6 suite **6 passed**（回帰なし）。
  3.2.1でBLE代表smoke **4 passed**（持ち込みNimBLE × IDF 5.4 coreの初の実機通過）、
  Classic 5 suite（inquiry / pairing / SPP stream / HID / A2DP media）**5 passed**。
- 版数guardの下限は3.2.0へ変更（それ未満は未測定のため`#error`のまま）。
- 3.2.xのcontrollerもPCM pathのため、**HFP audioの実機下限3.3.9は不変**です。

### 確定した対応範囲（2026-08-16）

| Core | IDF | compile/link | BLE実機 | Classic実機（HFP audio除く） | HFP audio |
| --- | --- | --- | --- | --- | --- |
| 3.2.0 | 5.4 | ✅ | — | — | 対象外（controller PCM） |
| 3.2.1 | 5.4 | ✅ | ✅ 4 passed | ✅ 5 passed | 対象外（同上） |
| 3.3.0 | 5.5 | ✅ | ✅ 4 passed | ✅ 5 passed | 対象外（同上） |
| 3.3.7 | 5.5.4 | ✅ | — | ✅ 2 passed（bt-mem修正後） | 対象外（controller PCM） |
| 3.3.8 | 5.5.4 | ✅ | ✅ 3 passed（bt-mem修正後） | HFP suiteで確認 | ✅（bt-mem修正後、SCO実流通） |
| 3.3.9 | 5.5.4 | ✅ | — | — | ✅ |
| 3.3.10 | 5.5.4 | ✅ | ✅ 4 passed | ✅ 6 suite | ✅ |
| 3.3.11 | 5.5.5 | ✅ | 全回帰 | ✅ 6 passed（契約vendor後の回帰） | ✅ |

## 残っているリスク

- L0のPASSは「buildできる」であって「動く」ではありません。archiveはIDFのheaderに従って
  struct layoutを焼き込んでいるので、layoutが変わった相手とlinkできてしまうと、症状は
  compile errorではなく実行時の異常として出ます。**linkが通ったversionは必ずL1まで進めます。**
- L2が落ちた場合、原因がCore側にある可能性があります。EspBle側を変更する前に、
  同じ古いCore同士（peerを2台とも古いCoreにする）で再現するかを確認します。
