# Classic-only Bluedroid host archiveの再生成

無印ESP32向けの`src/esp32/libespble_bluedroid_classic.a`を、ESP-IDFから再生成する手順です。
このarchiveは生成物であり、手修正しません。設定、link check、symbol変換、検査はすべて
`tools/build_classic_bluedroid_host.sh`から実行します。

## 固定する入力

| 項目 | 値 | 理由 |
|---|---|---|
| ESP-IDF | cleanな`v5.5.5` tag | Arduino-ESP32 3.3.11が基準にするIDFとABIを合わせる |
| target | `esp32` | Classic radioを持つ無印ESP32だけが対象 |
| compiler | xtensa-esp32 GCC 14.2.0 | Arduino-ESP32 3.3.11とtoolchain ABIを合わせる |
| Kconfig | `tools/classic_bluedroid_host/sdkconfig.defaults` | host-only、Classic-only、SPP/HID/SMPを固定する |

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
2. `main/link_check.c`を最終linkし、HCI attach、SPP、HID Device、HID Host APIの消失を検出する。
3. IDFの`esp-idf/bt/libbt.a`からglobal defined symbol一覧を作る。
4. `objcopy --redefine-syms`で全global defined symbolへ`espble_bd_` prefixを付ける。
5. debug情報をstripし、出力先へinstallする。
6. 名前空間化後の必須HCI/SPP/HID symbolを検査し、size、symbol数、SHA-256を表示する。

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
# CONFIG_BT_BLE_ENABLED is not set
```

controllerとBLE hostをarchiveへ含めないことが重要です。物理controllerはArduino側のHCI brokerが
所有し、BLEはEspBle同梱NimBLE hostが担当します。

## 再現性と差し替え判定

現在repositoryに格納しているarchiveの基準値は次です。

```text
size: 3035574 bytes
global defined symbols: 1788
sha256: 6b04833c2a1f32a04c357dca26b12ae05eb4f9ffdb14f57d49fc994abebf7a9f
```

2026-08-11に、`v5.5.5` tagの独立したclean worktreeと全submodule、GCC 14.2.0から一時生成し、
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

具体的なpytest commandは[テスト計画](../tests/TEST_PLAN.ja.md)と
[リリースチェックリスト](RELEASE_CHECKLIST.ja.md)を正とします。

## 配布形式について

現在はNimBLEをソース、Classic Bluedroidを`.a`で同梱しています。これは確定した最終形式では
ありません。両方をソースへ揃える案と、両方を再現可能archiveへ揃える案を、build時間、ABI互換、
debuggability、upstream化の差分量で比較する将来課題です。判断まではこの生成手順を正とし、
機能変更と配布形式変更を同時に行いません。
