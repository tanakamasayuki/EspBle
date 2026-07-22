# EspBle ドキュメント案内

> English: [README.md](README.md)

EspBleの開発ドキュメント一覧と、初見の人が状況を把握するための読む順序です。

## まず状況を把握する（初見の推奨順）

1. [../README.ja.md](../README.ja.md) — ライブラリの概要、対応/非対応チップ、はじめかた
2. [STATUS.ja.md](STATUS.ja.md) — **現在地**。検証状況・主な制限・1.0.0までの残作業
3. [DECISIONS.ja.md](DECISIONS.ja.md) — 確定した設計決定の台帳（「なぜこうなっているか」）
4. [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) / [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md) — 完了した複合HID API再設計の仕様

**迷ったら STATUS → DECISIONS の順**で読めば、現在地と背景がつかめます。

## 目的別インデックス

| 知りたいこと | 文書 |
|---|---|
| 今どこまで進み、次に何をするか | [STATUS.ja.md](STATUS.ja.md) |
| 何を作るライブラリか（要件・スコープ） | [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md) |
| どの機能が対応済み / 追加予定 / 対象外か | [FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md) |
| 設計思想・レイヤ構成 | [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) |
| 公開APIの設計方針 | [API_DESIGN.ja.md](API_DESIGN.ja.md) |
| 用語・命名規則 | [TERMINOLOGY.ja.md](TERMINOLOGY.ja.md) |
| 確定した設計決定とその理由 | [DECISIONS.ja.md](DECISIONS.ja.md) |
| HIDの仕様（Device / Host） | [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) / [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md) |
| スキャンからGATT通信までの入門 | [GUIDE_SCAN_TO_COMM.ja.md](GUIDE_SCAN_TO_COMM.ja.md) |
| テスト方針・カバレッジ | [../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md) |
| リリース前の確認手順 | [RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md) |
| Arduino-ESP32への修正依頼案 | [UPSTREAM_REQUEST_ARDUINO_ESP32_DESCRIPTOR_CONTEXT.ja.md](UPSTREAM_REQUEST_ARDUINO_ESP32_DESCRIPTOR_CONTEXT.ja.md) |
| ボード / coreビルド対応表（CI生成） | `BOARDS.<version>.md` / `COMPATIBILITY.<version>.md` |
| 使い方のサンプル | [../examples/README.ja.md](../examples/README.ja.md) |

## 文書の位置づけ

- **確定仕様（正）**: [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)、[DECISIONS.ja.md](DECISIONS.ja.md)、各SPEC。仕様の最終的な根拠。
- **現況の追跡**: [STATUS.ja.md](STATUS.ja.md)。進捗・制限・TODOを追う。まとまった作業ごとに更新。
- **CI生成物（手動編集しない）**: `BOARDS.<version>.md`、`COMPATIBILITY.<version>.md`。`.github/workflows/`の`board-matrix.yml`/`core-matrix.yml`が生成する。

## 現在の一言サマリ

**BLE共通基盤と複合HID Device / Hostは実装・Peer/unit検証済み**です。可能なHID拡張と相互運用確認を進め、初回公開版を1.0.0としてリリースする予定です。
