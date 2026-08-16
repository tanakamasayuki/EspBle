# Classic host配布形式の再評価（調査）

[PLAN_ESP32_CLASSIC.ja.md](PLAN_ESP32_CLASSIC.ja.md)の「配布形式の方針」は、再評価は保守上の
明確な利益が見えたときだけ行うと定めています。Coreバージョン互換の実測
（[PLAN_CORE_VERSION_MATRIX.ja.md](PLAN_CORE_VERSION_MATRIX.ja.md)）でその条件に達したため、
この文書が再評価の本体です。判断基準は現在の作業量ではなく**今後の保守量**です。

## 調査で確定した事実

配布形式の議論は「archiveはABIが怖い、sourceは重い」という一般論になりがちなので、
まず怖さの実体を測りました。

1. **archiveの外部依存は89 symbolだけ**で、全部が安定APIです。
   FreeRTOS 18 + Queue/Ringbuffer 8、libc 17、heap 8、esp_timer 7、NVS 5、log/assert 4、
   vfs 4、ほかesp_random / clock_gettime等。
2. **controller / VHCIへのimportはゼロ**です。HCI transportはEspBleのbrokerが
   `esp_bluedroid_attach_hci_driver`で外から差し込む設計のため、archiveはcontrollerに
   直接触れません。BLE系の未解決symbol（`BTM_Ble*`等）は`esp_gap_ble_api.c.obj`など
   最終linkに引き込まれない死蔵member内にあり、`pre_bump.py`のfinal-link gateが
   引き込まれないことを保証しています。
3. したがって、Core版の違いで実際に壊れていたのはimportではなく**compile時の契約**
   ——EspBleの`src/`がcoreのheaderから受け取る宣言と構造体レイアウト——です。
   3.2.x（IDF 5.4）の失敗はメンバ増減（`preferred_frame_size`等）、IDF 6の破壊的変更も
   フィールド削除・rename（[移行ガイド](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/release-6.x/6.0/bluetooth-classic.html)）で、同じ型の問題です。
4. その契約の実体は**15 headerで閉じます**。EspBleが使うBluedroid公開API 11本の
   include閉包は15本（legacy header 3本を含む）で、外への依存は`esp_err.h` /
   `sdkconfig.h` / `stdbool.h` / `stdint.h`のみ。この15本の中に**Classic APIのレイアウトを
   configで変える分岐はありません**（`CONFIG_*`分岐は2箇所、いずれもBLE側のmacro/enum）。
5. Bluedroid上流は**直近3ヶ月で100+ commits**（release/v5.5とmasterの双方でAPI取得上限に到達）。
   NimBLEと違い独立repoを持たないIDF内部componentなので、source化はfork保守になります。
   NimBLEのvendorは生成script + 手術的patch 5件で運用できている、という前例があります。
6. 実測した対応範囲（shim適用済みの現状）: build 3.3.0以上、BLE・Classic主要機能の実機
   3.3.0以上、HFP audioのみ3.3.9以上（3.3.0〜3.3.6はCoreのcontrollerがPCM pathでbuild
   されているため。3.3.8はEspBle側の疑いがある排他問題で、独立の調査項目）。
   3.2.x（IDF 5.4）はbuild不可。
7. Arduinoのprecompiled解決は`src/<mcu>/`の1枠だけで、Core版数によるarchiveの
   出し分けはできません。

## 案の比較

### 案A: 3.3.11のみにpin（shim以前の状態）

- 利点: 検証対象が1点。契約ずれが原理的に起きない。
- 欠点: Coreがreleaseされるたびに実際は動く版を`#error`で拒否する。利用者の環境選択を
  1版に固定する不便が恒久化する。
- 限界: IDF 6系Coreが出た時点でClassicはその世代で提供不能。再生成すると5.5系を切る。

### 案B: archive + 互換shim（現状。commit済み）

`EspBleClassicCoreCompat.h`の3 shim（全て3.3.11では`#ifndef`で無効）で、IDF 5.5世代
（Core 3.3.0〜3.3.11）を1つのarchiveで支えます。

- 利点: 実測済み。追加保守はCore releaseごとのmatrix実行のみで、コードは変わらない。
- 欠点: 契約はcoreのheaderから来たままなので、**新しいCoreが出るたびにずれの可能性が残る**。
  shimは「起きたずれへの後追い」であって免疫ではない。
- 限界: IDF 6系Coreで契約が壊れる（フィールド削除はshimで埋められない——埋めると
  レイアウトについて嘘をつくことになる）。archiveは1世代分なので、6対応で再生成すると
  5.5系Coreを切り捨てるジレンマは案Aと同じ。

### 案C: 全source vendor（NimBLE方式）

`tools/build_classic_bluedroid_host.sh`がbuildしている290 .c（bluedroid host 249、
tinycrypt 16、osi 15、btc 6ほか）を、`vendor_nimble_esp32.py`と同型の生成scriptで
`src/`へ取り込みます。

- 実装の要点（NimBLEの前例がそのまま使える）:
  - config注入: vendor時に各.cへ`#include "espble_bd_config.h"`を挿入
    （NimBLEの`espble_nimble_config.h`と同じ手口）。coreの`sdkconfig.h`より
    EspBleの要求値（HID有効、external codec等）を優先させる。
  - namespace: `objcopy --redefine-syms`の代わりに、defined symbol 2796個の
    `#define`群を生成headerとして同挿入する。compiler flagはsketch側に要求しない。
  - release gateの置換: `verify_classic_archive.py`のSHA照合をvendor来歴照合へ、
    final-link gateは維持。
- 利点: **IDF世代を跨げる唯一の完全解**。持ち込むtreeが自前のheaderで閉じるため、
  coreがIDF 6でも「89相当の安定API」が保たれる限りcompileできる（NimBLEが5.4/5.5
  両世代で実証済みのモデル）。IDF 6のAPI renameへの追随も呼び出し側の修正だけ
  （EspBleの呼び出しは既に6の新名称）。
- 欠点: fork保守。上流は月30+ commitsペースで動き、独立upstreamが無いので
  再pinのたびにvendor scriptとpatchの再適用・再検証になる。クリーンbuildが
  +290ファイル。gateの再構築。
- 限界: controller configの問題（下記）は解決しない。上流修正が自動では届かない点は
  archiveと同じ（pinの宿命）。

### 案D: archive + 契約headerのvendor（「変わりやすい所を手元に置く」の最小形）

archiveはそのまま、**契約の15 headerをv5.5.5からvendor**し、`src/`のincludeを
自前パス（例: `src/esp32/include/`）へ張り替えます。coreの同名headerとの衝突は
起きません——includeを自前名にするだけで、coreのheaderはそもそも読まれなくなります。

- 利点:
  - 宣言とレイアウトが**常にarchiveのbuild時と一致**する。coreのheader変更に対する免疫で、
    案Bの「後追いshim」が原理的に不要になる（既存3 shimはvendored headerに吸収）。
  - 事実1・2により、残る結合は安定API 89個だけ。**IDF 6系Coreでもarchiveがそのまま
    動く可能性**があり、これは測定可能な仮説になる。
  - 3.2.x（IDF 5.4）もheader起因の失敗は消えるため、build可否を再測定する価値が出る。
  - 作業が小さく、可逆。vendorする15本はMANIFESTへhash記録すれば来歴gateも既存の延長。
- 欠点: headerと.aの二重管理（再生成時に必ず対で更新する運用が要る。gateで強制可能）。
  上流修正が届かない点は同じ。
- 限界: coreがFreeRTOS/NVS等の基盤ABIを変えたら再生成が要る（可能性は低いが、
  そのときは案Cでも同じ影響を受ける）。controller configの問題は解決しない。

### 案E: 世代別に複数archiveを同梱して呼び分け

`espble_bd55_*` / `espble_bd6_*`のようにprefixを世代別にし、`ESP_ARDUINO_VERSION`で
呼び分ける案。linkerのmember granularityで未参照側はELFに載りません。

- 欠点が並ぶため劣後: zipが世代ごとに+4.6MB、release gateが世代数ぶん倍増、
  世代ごとのtoolchain（GCCが違う）を保持してclean再生成。案Dが同じ目的を
  はるかに小さく達成します。

## どの案でも残る限界

- **controllerはCoreのprebuilt binary**です。HFP audioがHCI pathを使えるのは
  controllerが`CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI`でbuildされた3.3.8以降だけ、
  という類の差は、host側の配布形式をどう変えても動きません。
- **pinする以上、上流のBluedroid修正（セキュリティ修正を含む）は再pinまで届きません**。
  直近3ヶ月100+ commitsの中には不具合修正が多数含まれます。再pinの頻度を決めるのは
  配布形式ではなく運用方針です（作業はB/Dが再build、Cが再vendorで、どちらもscript化可能）。
- 3.3.8で観測したHFP役割排他の不発は、`activateHfpAg()`というEspBle内部の判定の
  問題である疑いがあり、配布形式と独立に調査が必要です。
- BLE側（持ち込みNimBLE）は既にsourceで、5.4 / 5.5両世代のbuild実績があります。
  この文書の対象はClassic hostだけです。

## 保守シナリオでの比較

| これから起きること | B（現状） | C（全source） | D（archive+契約header） |
| --- | --- | --- | --- |
| Core 3.3.12（IDF 5.5.x）が出る | matrix実行。headerがずれたらshim追加 | matrix実行のみ | matrix実行のみ（契約は自前なのでずれない） |
| Core 4.x（IDF 6）が出る | Classic不成立。`#error`で塞ぐか、再生成して5.5系を切る | 呼び出し側修正で両世代を維持できる見込み | **archiveのまま動くか実測できる**。破綻したらCへ移行 |
| 上流に重要な修正が入る | 再build（script済み） | 再vendor + patch再適用 | 再build + header再vendor（対で更新） |
| IDF 7級の大改変 | 再生成 + 契約総点検 | fork追随の作業が最大 | Cと同じ判断点に立つ |

## 推奨

**今回のrelease: B（済）に加えてDを実施。Cは実施せず、移行判断の引き金だけ決めておく。**

- Dを今やる理由: 作業が小さい割に、案Bの弱点（新Coreごとの後追い）を原理的に消し、
  IDF 6系Coreへの生存可能性という測定可能な選択肢を残すためです。既存のshim 3件も
  vendored headerに吸収されて消えます。
- Cを今やらない理由: 作業量ではなく順序の問題です。IDF 6でBluedroidがどう変わるかが
  確定する前にfork化すると、6対応の再vendorで同じ作業を二度行うことになります。
  Dの実装（vendor script、来歴gate、config照合）はCへ移行する場合の下準備の大半を
  兼ねます。
- Cへの移行判断: IDF 6系Coreのpreviewが出た時点でDのまま実測し、archiveが成立しない
  ことが確認されたら、**その時点のIDF**からvendorしてCへ移行します。

この推奨を採る場合の実施順は、(1) 15 headerのvendorとinclude張り替え、(2) MANIFESTと
`verify_classic_archive.py`へのheader hash追加、(3) 3.3.0〜3.3.11のmatrix再実行と
3.2.xの再測定、(4) 文書反映、です。

## 実施結果（2026-08-16）

案Dを実装し、想定を上回る結果になりました。

- `tools/vendor_classic_contract.py`が15 headerを`src/esp32/include/`へvendor
  （IDF v5.5.5とbyte一致、hashで照合可能）。`src/`のbluedroid API include 14箇所を
  自前パスへ張り替え、shim 3件（`EspBleClassicCoreCompat.h`）は吸収して削除。
  `verify_classic_archive.py`はheaderのinventory / hash / `include/`実体の三重照合を行い、
  `build_classic_bluedroid_host.sh`は再生成時に同じIDFからheaderを再vendorする。
- **IDF 5.4世代（Core 3.2.x）が対応範囲に入りました。** 契約が自前になると3.2.xの
  compile失敗は発生源ごと消え、残った差はlink時のundefined **`esp_log` 1個**
  （IDF 5.5で入ったlog v2の入口）だけでした。`EspBleClassicLogCompat.c`（3.3.0未満の
  coreでだけ実体を持つ30行の転送）で解消。「archiveは1つのIDF世代しか支えられない」
  という本文書の前提は、契約をvendorしない場合にだけ成り立つ制約だったことになります。
- 実測: compile matrixは3.2.0〜3.3.11で全pass。実機は3.3.11の出荷状態で回帰6 passed、
  3.2.1でBLE 4 passed + Classic 5 passed。確定表は
  [core版数のテスト計画](PLAN_CORE_VERSION_MATRIX.ja.md)にあります。
- HFP audio（3.3.9未満のcontrollerがPCM path）はどの配布形式でも動かない領域として確定。

IDF 6系Coreへの生存可能性は、これで「仮説」から「有望な仮説」になりました。5.4と5.5の
世代差がheader契約 + `esp_log` 1個で吸収できた以上、6でも同じ規模である可能性が
高いためです。判断の引き金（6系Core previewでの実測、破綻時のC移行）は変わりません。
