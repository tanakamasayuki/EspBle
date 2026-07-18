# EspBle ドキュメント案内

> English: [README.md](README.md)

EspBleの開発ドキュメント一覧と、初見の人が状況を把握するための読む順序です。

## まず状況を把握する（初見の推奨順）

1. [../README.ja.md](../README.ja.md) — ライブラリの概要、対応/非対応チップ、はじめかた
2. [STATUS.ja.md](STATUS.ja.md) — **現在地**。実装済み・主な制限・次の作業（P0）。ここが起点
3. [DECISIONS.ja.md](DECISIONS.ja.md) — 確定した設計決定の台帳（「なぜこうなっているか」）
4. 進行中の主作業 → [HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md) — 初回リリースの主目玉（HID複合対応・API再設計）の作業リスト

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
| HID Keyboardの仕様（Device / Host） | [HID_KEYBOARD_DEVICE_SPEC.ja.md](HID_KEYBOARD_DEVICE_SPEC.ja.md) / [HID_KEYBOARD_HOST_SPEC.ja.md](HID_KEYBOARD_HOST_SPEC.ja.md) |
| 進行中のHID再設計計画 | [HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md) |
| 開発フェーズの全体像 | [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) |
| テスト方針・カバレッジ | [../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md) |
| ボード / coreビルド対応表（CI生成） | `BOARDS.<version>.md` / `COMPATIBILITY.<version>.md` |
| 使い方のサンプル | [../examples/README.ja.md](../examples/README.ja.md) |

## 文書の位置づけ

- **確定仕様（正）**: [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)、[DECISIONS.ja.md](DECISIONS.ja.md)、各SPEC。仕様の最終的な根拠。
- **現況の追跡**: [STATUS.ja.md](STATUS.ja.md)。進捗・制限・TODOを追う。まとまった作業ごとに更新。
- **進行中の計画（未確定を含む）**: [HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md)、[DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md)。
- **CI生成物（手動編集しない）**: `BOARDS.<version>.md`、`COMPATIBILITY.<version>.md`。`.github/workflows/`の`board-matrix.yml`/`core-matrix.yml`が生成する。

## 現在の一言サマリ（2026-07-18時点）

初回リリース(0.1.0)へ向けた設計固めフェーズ。**HID API再設計（複合HID + EspUsbDevice/Host流のAPI）を初回リリースに含める**と決定し、[HID_REDESIGN_PLAN.ja.md](HID_REDESIGN_PLAN.ja.md)のPhase 0（設計確定）まで完了。次はPhase 1（Device backend一元化）からテストファーストで実装する段階。詳細はSTATUSを参照。
