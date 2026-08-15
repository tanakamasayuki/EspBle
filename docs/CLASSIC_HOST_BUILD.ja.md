# Classic-only Bluedroid host archiveの再生成

無印ESP32向けの`src/esp32/libespble_bluedroid_classic.a`を、ESP-IDFから再生成する手順です。
このarchiveは生成物であり、手修正しません。設定、link check、symbol変換、検査はすべて
`tools/build_classic_bluedroid_host.sh`から実行します。

## 固定する入力

| 項目 | 値 | 理由 |
|---|---|---|
| ESP-IDF | cleanな`v5.5.5` tag | 対応対象をArduino-ESP32 3.3.11に限定し、そのIDFとABIを合わせる |
| target | `esp32` | Classic radioを持つ無印ESP32だけが対象 |
| compiler | xtensa-esp32 GCC 14.2.0 | Arduino-ESP32 3.3.11とtoolchain ABIを合わせる |
| Kconfig | `tools/classic_bluedroid_host/sdkconfig.defaults` | host-only、Classic-only、profileとexternal codec境界を固定する |

スクリプトはIDF tag、dirty checkout、compiler versionを検査し、一致しなければ生成を中止します。
ESP-IDFやArduino Coreを更新するときは、archiveだけを作り直さず、ABI、公開symbol、実機回帰を
まとめて再検証します。

## 初回セットアップ

作業用ESP-IDFはEspBle repositoryの外へ置きます。

```sh
git clone --branch v5.5.5 --recursive \
  https://github.com/espressif/esp-idf.git /path/to/esp-idf-v5.5.5
cd /path/to/esp-idf-v5.5.5
./install.sh esp32
. ./export.sh
git describe --tags --always --dirty
xtensa-esp32-elf-gcc -dumpfullversion
```

最後の2行はそれぞれ`v5.5.5`と`14.2.0`でなければなりません。submoduleを省略したcheckoutや、
patchを当てたIDFはこの生成手順の入力にしません。IDF側の変更が必要になった場合はpatchをEspBle側で
追跡可能にしてから、固定入力と検証記録を更新します。

## 生成

EspBle repository rootで実行します。

```sh
. /path/to/esp-idf-v5.5.5/export.sh
tools/build_classic_bluedroid_host.sh
```

既定の出力先は`src/esp32/libespble_bluedroid_classic.a`です。別の場所へ出す場合は第1引数、build
directoryを分離する場合は`ESPBLE_CLASSIC_HOST_BUILD_DIR`を使います。

```sh
work_dir=$(mktemp -d)
ESPBLE_CLASSIC_HOST_BUILD_DIR="$work_dir/build" \
  tools/build_classic_bluedroid_host.sh "$work_dir/libespble_bluedroid_classic.a"
```

スクリプトは次の処理を順に行います。

1. `tools/classic_bluedroid_host/`をESP-IDF projectとしてbuildする。
2. `main/link_check.c`を最終linkし、HCI attach、SPP、HID Device/Host、A2DP Sink/Source、
   AVRCP CT/TG、HFP Client/AGと各external media APIの消失を検出する。
3. IDFの`esp-idf/bt/libbt.a`からglobal defined symbol一覧を作る。
4. `objcopy --redefine-syms`で全global defined symbolへ`espble_bd_` prefixを付ける。
5. debug情報をstripし、出力先へinstallする。
6. 全global defined symbolが名前空間化されたことと必須profile symbolを検査し、size、symbol数、
   SHA-256を表示する。

必須symbolにはHFP AGのcodec preferenceを保持する名前空間化済み
`espble_bd_btc_conf_hf_force_wbs`も含む。ESP-IDFの公開AG APIにはcodec選択関数がないため、EspBleの
`preferredAudioCodec`はこのprocess-wide設定をSLC前に切り替え、その後は標準`+BAC/+BCS` negotiationへ委ねる。
Client/AGはprocess-wideで排他なので、別HFP roleとの競合は発生しない。

FreeRTOS、NVS、timer、loggingなどarchiveが参照するundefined symbolは変換しません。これらは
Arduino-ESP32から解決します。archive自身が定義するsymbolだけを名前空間化することで、core内蔵の
Bluedroidと衝突せず、独自host APIが誤ってcore側へ解決されることを防ぎます。

## 固定Kconfig

最低限、生成後の`build/sdkconfig`が次を満たすことを確認します。

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_CONTROLLER_DISABLED=y
CONFIG_BT_CLASSIC_ENABLED=y
CONFIG_BT_SPP_ENABLED=y
CONFIG_BT_HID_ENABLED=y
CONFIG_BT_HID_HOST_ENABLED=y
CONFIG_BT_HID_DEVICE_ENABLED=y
CONFIG_BT_SMP_ENABLE=y
CONFIG_BT_SMP_CRYPTO_STACK_TINYCRYPT=y
CONFIG_BT_A2DP_ENABLE=y
CONFIG_BT_A2DP_USE_EXTERNAL_CODEC=y
CONFIG_BT_HFP_ENABLE=y
CONFIG_BT_HFP_CLIENT_ENABLE=y
CONFIG_BT_HFP_AG_ENABLE=y
CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=y
CONFIG_BT_HFP_USE_EXTERNAL_CODEC=y
CONFIG_BT_HFP_WBS_ENABLE=y
# CONFIG_BT_BLE_ENABLED is not set
```

controllerとBLE hostを設定上無効にすることが重要です。ESP-IDFのcomponent source選択により、
未使用のBLE名objectがstatic archiveへ残ることはありますが、Classic-onlyの最終linkでは抽出されません。
物理controllerはArduino側のHCI brokerが所有し、BLEはEspBle同梱NimBLE hostが担当します。

## 再現性と差し替え判定

現在repositoryに格納しているarchiveの基準値は次です。

```text
size: 4596952 bytes
global defined symbols: 2796
sha256: d64d3a40a3f598e206c5aaf798e9d8fda5c867b632224ed72a616b1221089421
```

機械可読の正本はarchive横の[`MANIFEST.json`](../src/esp32/MANIFEST.json)です。artifactだけでなく、
IDF commit、toolchain、build input、member / symbol集合、license inventoryも固定しています。

2026-08-11に、`v5.5.5` tagの独立したclean cloneと全submodule、GCC 14.2.0から一時生成し、
このsize・symbol数・SHA-256を再現した。格納済みarchiveとの`cmp`も一致し、差し替えは不要だった。

一時出力と格納済みarchiveを比較する例です。

```sh
cmp src/esp32/libespble_bluedroid_classic.a \
  "$work_dir/libespble_bluedroid_classic.a"
sha256sum src/esp32/libespble_bluedroid_classic.a \
  "$work_dir/libespble_bluedroid_classic.a"
```

入力を変えていないのに一致しない場合は差し替えません。IDF、compiler、Kconfig、生成スクリプト、
archive member順、symbol一覧を調べます。意図して入力を更新した場合は、この基準値、Classic計画、
CHANGELOG、Arduino Core互換表を同時に更新します。

## archive更新後の必須確認

1. `classic_spp_exclusive`、`classic_hid_profiles`、`classic_hid_report`を無印ESP32 2台で実行する。
2. `dual_host_smoke`と`dual_host_rpa`を無印ESP32 2台で実行する。
3. 通常の無印ESP32 NimBLE回帰をCentral / Peripheral両roleで実行する。
4. ESP32-S3のCompileSmokeを実行し、Classic archiveが他SoCへリンクされないことを確認する。
5. `nm`で必須prefixed symbolと、意図しないunprefixed global defined symbolがないことを確認する。
6. `python tools/verify_classic_archive.py`を実行し、manifest、license、build inputとarchiveの一致を確認する。

具体的なpytest commandは[テスト計画](../tests/TEST_PLAN.ja.md)と
[リリースチェックリスト](RELEASE_CHECKLIST.ja.md)を正とします。

## 配布形式について

次回Classic拡張ではNimBLEをsource、Classic Bluedroidを`.a`で同梱するmixed distributionを維持します。
NimBLEはlocal patchとtarget条件をsourceで追跡し、ESP-IDF component依存の大きいBluedroidは固定Kconfigから
再現可能なarchiveとして生成します。形式を揃えること自体はrelease条件ではありません。

Classic archiveに含まれるESP-IDF / TinyCrypt由来コードのライセンス、帰属表示とEspBleによる変更内容は、
archive横の[`NOTICE`](../src/esp32/NOTICE)と[`LICENSES/`](../src/esp32/LICENSES/)を正本とします。

A2DP/AVRCP/HFPを追加したarchiveの設定と必須symbolは
[Classic Audio拡張計画](PLAN_ESP32_CLASSIC_AUDIO.ja.md)を正本とします。A2DPはexternal codec、HFPは
Voice over HCI / external codecで生成し、cover art / GOEPは無効です。上記基準値はこのAudio設定を
含む格納済みarchiveの値です。
