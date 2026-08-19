# リリースチェックリスト

> English: [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md)

EspBleをリリースする前の確認項目です。GitHub Actionsと`tools/`のbump scriptは共通release toolkit由来のため、通常のリリース作業では編集しません。

## 事前確認

- `README.ja.md` / `README.md`、`docs/STATUS.ja.md` / `docs/STATUS.md`、`docs/FEATURE_MATRIX.ja.md` / `docs/FEATURE_MATRIX.md`の対応範囲が実装と一致している。
- 利用者向け文書の日本語版と英語版が同期している（root `README`、`docs/README`、`docs/GUIDE_BLE_BASICS`、`docs/GUIDE_CLASSIC_BASICS`、`docs/GUIDE_ADVANCED`、`docs/GUIDE_MIGRATION`、`docs/GUIDE_HID_DESCRIPTORS`、`docs/API_DESIGN`、`docs/CLASSIC_VS_BLE`、`docs/STATUS`、`docs/FEATURE_MATRIX`、`docs/RELEASE_CHECKLIST`、`tests/TEST_PLAN`、`examples/README` と各example README）。
- `docs/API_DESIGN.md` / `docs/API_DESIGN.ja.md`、`docs/HID_DEVICE_SPEC.ja.md`、`docs/HID_HOST_SPEC.ja.md`が公開APIと一致している。
- `examples/README.ja.md` / `examples/README.md`と各example READMEが実装済みAPIと一致している。
- 完了済みの作業計画や古いAPI名へのリンクが残っていない。
- `CHANGELOG.md`の`Unreleased`に今回の利用者向け変更が記録されている。

## メタデータ

- `library.properties`の`name`、`version`、`sentence`、`paragraph`、`architectures`、`includes`が公開内容と一致している。
- `keywords.txt`に主要class、report/event型、accessor、callback/listener APIが含まれている。
- 生成済みの`docs/BOARDS.<version>.md` / `docs/COMPATIBILITY.<version>.md`がリリース対象versionと現在のexample集合を反映している。
- Classicの利用者向け文書が実測済みの対応範囲（Core 3.2.0以上、HFP audioのみ3.3.8以上、
  archiveはESP-IDF 5.5.5 / GCC 14.2.0でbuildし契約headerを同梱）で一致している。
- release zipにrootの`THIRD_PARTY_NOTICES.md`、Classic archive横の`NOTICE` / `MANIFEST.json` / `LICENSES/`、NimBLE横の`LICENSE` / `NOTICE`がすべて入り、manifestのlicense inventoryと一致している。
- Classicをrelease対象へ含める場合、[archive再生成手順](CLASSIC_HOST_BUILD.ja.md)どおりcleanなESP-IDF v5.5.5 / GCC 14.2.0から一時生成し、格納済み`libespble_bluedroid_classic.a`とのSHA-256一致、必須prefixed symbol、最終ESP32 link、他SoC非リンクを確認する。

## 自動テスト

まずESP32-S3を2台接続して標準の全回帰を実行します。ライブラリ更新後やprofile切替後はstale build cacheを避けるため`--clean`を付けます。

```sh
cd tests
uv run --env-file .env pytest --clean
```

`rpa_bond`は[テスト計画](../tests/TEST_PLAN.ja.md)に記録した理由で無印ESP32専用です。`--profile`を指定しないと各sketchの`default_profile`が使われるため、この実行ではS3ではなく無印ESP32ペアで動きます。ペアを外していればportが無いのでskipされます。

次に無印ESP32の2台（portは`.env`の`TEST_SERIAL_PORT_ESP32_PEER_HOST` / `TEST_SERIAL_PORT_PEER_DEVICE_ESP32_PEER_DEVICE`）で、同梱NimBLE host（`src/nimble_esp32/`）を役割ごとに掃引します。無印ESP32はhostがcore同梱ではなく**EspBleが持ち込んだもの**なので、`src/`へ変更が入るリリースでは必ず実行します。所要は各1時間程度です。

```sh
# 無印ESP32を親側(Central)に
uv run --env-file .env pytest --clean peer/ \
  --ignore=peer/classic_hid_profiles --ignore=peer/classic_a2dp_sink_profile \
  --ignore=peer/classic_radio_settings \
  --profile esp32_peer_host --peer-profile device:s3_peer_device

# 無印ESP32をPeer(Peripheral)に
uv run --env-file .env pytest --clean peer/ \
  --ignore=peer/classic_hid_profiles --ignore=peer/classic_a2dp_sink_profile \
  --ignore=peer/classic_radio_settings \
  --profile s3_peer_host --peer-profile device:esp32_peer_device
```

その役割のesp32 profileを持たないsuiteは自動的にskipされます。skipされるのは、core同梱`BLE`
ラッパで書かれた側（無印ESP32ではラッパがBluedroidになり実行不可）と、2M PHYを要求する
`phy_update`です。`--ignore`の3つはpeer sketchを持たない単独suiteで、skipではなく
「unknown peer」のerrorになるため除外します（後述のClassic単独commandで実行します）。

`src/`に触れない文書だけの変更では、代表suiteのsmokeで足ります（両役割で15分程度）。

```sh
uv run --env-file .env pytest --clean \
  peer/gatt_read_write/ peer/security_bond/ peer/hid_keyboard_host/ \
  peer/mtu/ peer/connection_parameters/ \
  --profile esp32_peer_host --peer-profile device:s3_peer_device
```

除外の理由と実行頻度は[テスト計画](../tests/TEST_PLAN.ja.md#無印esp32回帰)、方針と検証記録は[無印ESP32対応計画](PLAN_ESP32.ja.md)を参照します。portはpytestが排他で掴むので、別のpytestを同時に走らせても待ち合わせになります（`arduino-cli upload`や`esptool`を直接使うと待たずに失敗するので使わないでください）。

Classicをrelease対象へ含める場合は、同じ無印ESP32 2台でClassic専用構成とdual-hostを追加実行します。

```sh
# peer sketchを持たない単独suiteは分けて実行する。
uv run --env-file .env pytest --clean -s \
  peer/classic_hid_profiles/ peer/classic_a2dp_sink_profile/ \
  peer/classic_radio_settings/ \
  --profile esp32_peer_host

uv run --env-file .env pytest --clean -s \
  peer/classic_inquiry/ peer/classic_pairing/ \
  peer/classic_spp_exclusive/ peer/classic_spp_stream/ \
  peer/classic_core_host_spp/ \
  peer/classic_hid_api/ peer/classic_hid_control/ \
  peer/classic_hid_report/ peer/classic_hid_gamepad/ \
  peer/classic_a2dp_media/ peer/classic_hfp_client/ \
  peer/classic_hfp_cvsd/ \
  peer/dual_host_smoke/ peer/dual_host_rpa/ peer/dual_host_hfp/ \
  peer/dual_host_a2dp/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device
```

`classic_hid_profiles`のようにpeer sketchを持たないsuiteへ`--peer-profile device:...`を渡すと、
pytestがunknown peerとして拒否します。上記2 commandを一括化しないでください。

数時間級soakは[引き継ぎ](HANDOFF_ESP32_CLASSIC.ja.md)の条件でコードfreeze前に完走させます。release
candidateでは上記の通常回帰を再実行し、soak log、heap、broker diagnosticsを技術検証記録へ残します。

次にP4+C6 fixtureとPeer側S3を接続し、ESP-Hosted代表suiteを実行します。P4+C6は常時接続不要ですが、リリース候補では必須です。ESP32-P4-Function-EV-Board、または汎用`esp32p4` variantと同じ標準SDIO配線を基準とし、独自配線では使用したvariantまたはpin上書きを記録します。

```sh
uv run --env-file .env pytest --clean \
  peer/stack_smoke/ peer/connect_disconnect/ peer/gatt_read_write/ \
  peer/notify_indicate/ peer/mtu/ peer/wifi_ble_coexistence/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

P4 fixtureの条件とpinは[ESP-Hostedセットアップ](ESP_HOSTED_SETUP.ja.md)、実行頻度と必須合格範囲は[テスト計画](../tests/TEST_PLAN.ja.md#p4c6-esp-hosted回帰)を参照します。[既知制限](ESP_HOSTED_LIMITATIONS.ja.md)に該当するSecurityと完全な初期化・終了反復は現在のrelease gateには含めませんが、CoreまたはC6 firmwareを更新した場合は再実行して解消の有無を確認します。

無印ESP32のcore版数gate（実機不要）。対応範囲は3.2.0〜3.3.11で、契約headerの同梱により
Coreの版でずれない設計ですが、releaseごとに実測で確認します。全cellがpassすることが合格です。
実測記録は[core版数のテスト計画](PLAN_CORE_VERSION_MATRIX.ja.md)にあります。

```sh
python3 tools/version_matrix.py \
  --core-versions 3.2.0,3.2.1,3.3.0,3.3.11 --targets esp32 \
  --examples CompileSmoke,Gap/Connect,Security/StaticPasskeyServer,Classic/SppServer,Classic/HidKeyboard,Classic/A2dpSinkAvrcp,Classic/HfpClientRaw \
  --output /tmp/esp32_core_matrix.md
```

`docs/COMPATIBILITY.<version>.md`の無印ESP32列は、Classicが`src/`へ入る前の結果のままです。
releaseでは`core-matrix.yml`を回して再生成し、この列が3.2.0以上で✅になることを確認します。

全exampleのcompile。Classic exampleは無印ESP32専用で`esp32s3` profileを持たないため、
CIの`compile-examples.yml`と同じく各sketchの`default_profile`へfallbackします。

```sh
set -uo pipefail
for sketch in $(find examples -name sketch.yaml -printf '%h\n' | sort); do
  profile=esp32s3
  grep -q "^  esp32s3:" "$sketch/sketch.yaml" ||
    profile=$(grep -oP '^default_profile:\s*\K\S+' "$sketch/sketch.yaml")
  arduino-cli compile --profile "$profile" "$sketch"
done
```

- リリース直前にS3 Peer suiteを複数回実行し、flaky failure、heap低下、task残留がないことを確認する。P4代表suiteは最終候補で少なくとも1回通過させる。

手元のloopで確認するのはesp32とesp32s3です。release対象board全体でのexample compileは
`compile-examples.yml`の仕事で、`board-matrix.yml` / `core-matrix.yml`と同じく後述の「workflow」で扱います。

## 手動相互運用

自動Peerテストとは別に、結果を実施日・OS/機器versionとともに記録します。

- HID Deviceを少なくとも2種類の外部Host実装（例: AndroidとLinux）へ接続し、keyboard、mouse、consumer control、再接続を確認する。
- HID Hostを少なくとも1台の市販BLE keyboardへ接続し、入力、modifier、LED、切断release、Bond再接続を確認する。
- Just Worksと静的passkeyを外部BLE実装から確認する。
- Scan、GATT read/write、notifyの基本経路をスマートフォンまたはPCのBLE toolで確認する。

## 最終確認

- `python tools/verify_classic_archive.py`でarchive、manifest、license、build input、symbol inventoryの固定値が一致する。
- `python tools/release_hooks/pre_bump.py`でClassic host呼出しがすべてnamespace化され、ESP32最終ELFが同梱hostだけを使い、非ESP32のmapがarchiveを含まないことを確認する。
- `git diff --check`とリンク検索を行い、意図しないbuild artifact、cache、local profile固有の変更がないことを確認する。

## workflow

次はGitHub Actionsで実行します。手元で先に回しても結果が変わらないため、gateではなくrelease時の
手順として扱い、結果だけを記録します。

- `board-matrix.yml`: 対象boardを再生成し、`docs/BOARDS.<version>.md`を確定する。
- `core-matrix.yml`: Arduino-ESP32対応versionを再検証し、`docs/COMPATIBILITY.<version>.md`を確定する。
- `compile-examples.yml`: Classic archiveのintegrity / source namespace / final-link gate後、release対象board全体でexample compile。
- `release.yml`: pre-bump hookで同じClassic gateを通した後、bump scriptのpreviewでversion変更を確認し、version、CHANGELOG、release branch、tag、GitHub releaseを作成する。
- 公開後にArduino Library Managerから取得できるversionと最小exampleのcompileを確認する。
