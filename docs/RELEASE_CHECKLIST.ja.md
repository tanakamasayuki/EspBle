# リリースチェックリスト

EspBleをリリースする前の確認項目です。GitHub Actionsと`tools/`のbump scriptは共通release toolkit由来のため、通常のリリース作業では編集しません。

## 事前確認

- `README.ja.md` / `README.md`、`docs/STATUS.ja.md`、`docs/FEATURE_MATRIX.ja.md`の対応範囲が実装と一致している。
- `docs/API_DESIGN.ja.md`、`docs/HID_DEVICE_SPEC.ja.md`、`docs/HID_HOST_SPEC.ja.md`が公開APIと一致している。
- `examples/README.ja.md` / `examples/README.md`と各example READMEが実装済みAPIと一致している。
- 完了済みの作業計画や古いAPI名へのリンクが残っていない。
- `CHANGELOG.md`の`Unreleased`に今回の利用者向け変更が記録されている。

## メタデータ

- `library.properties`の`name`、`version`、`sentence`、`paragraph`、`architectures`、`includes`が公開内容と一致している。
- `keywords.txt`に主要class、report/event型、accessor、callback/listener APIが含まれている。
- 生成済みの`docs/BOARDS.<version>.md` / `docs/COMPATIBILITY.<version>.md`がリリース対象versionと現在のexample集合を反映している。

## 自動テスト

ESP32-S3を2台接続し、ライブラリ更新後やprofile切替後はstale build cacheを避けるため`--clean`を付けます。

```sh
cd tests
uv run --env-file .env pytest --clean
```

全exampleのESP32-S3 compile:

```sh
set -euo pipefail
for sketch in $(find examples -name sketch.yaml -printf '%h\n' | sort); do
  arduino-cli compile --profile esp32s3 "$sketch"
done
```

- `compile-examples.yml`が全exampleをESP32-S3で通過している。
- `board-matrix.yml` / `core-matrix.yml`を手動実行し、生成文書を更新する。
- リリース直前にPeer suiteを複数回実行し、flaky failure、heap低下、task残留がないことを確認する。

## 手動相互運用

自動Peerテストとは別に、結果を実施日・OS/機器versionとともに記録します。

- HID Deviceを少なくとも2種類の外部Host実装（例: AndroidとLinux）へ接続し、keyboard、mouse、consumer control、再接続を確認する。
- HID Hostを少なくとも1台の市販BLE keyboardへ接続し、入力、modifier、LED、切断release、Bond再接続を確認する。
- Just Worksと静的passkeyを外部BLE実装から確認する。
- Scan、GATT read/write、notifyの基本経路をスマートフォンまたはPCのBLE toolで確認する。

## 最終確認とリリース

- `git diff --check`とリンク検索を行い、意図しないbuild artifact、cache、local profile固有の変更がないことを確認する。
- bump scriptのpreviewでversion変更を確認する。
- release workflowでversion、CHANGELOG、release branch、tag、GitHub releaseを作成する。
- 公開後にArduino Library Managerから取得できるversionと最小exampleのcompileを確認する。
