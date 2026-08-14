# EspBle ドキュメント案内

> English: [README.md](README.md)

EspBleの開発ドキュメント一覧と、初見の人が状況を把握するための読む順序です。

## まず状況を把握する（初見の推奨順）

1. [../README.ja.md](../README.ja.md) — ライブラリの概要、対応/非対応チップ、はじめかた
2. [STATUS.ja.md](STATUS.ja.md) — **現在地**。検証状況・主な制限・次回リリースまでの残作業
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
| BLE通信の入門ガイド（GAP / セキュリティ / GATT / UUID / HID / BLE MIDI） | [GUIDE_BLE_BASICS.ja.md](GUIDE_BLE_BASICS.ja.md) |
| BLEとClassicのどちらを使うか、両方にある機能の差 | [CLASSIC_VS_BLE.ja.md](CLASSIC_VS_BLE.ja.md)（英語版: [CLASSIC_VS_BLE.md](CLASSIC_VS_BLE.md)） |
| Classic通信の入門ガイド（inquiry / 無線設定 / SPP / security / HID / A2DP / HFP） | [GUIDE_CLASSIC_BASICS.ja.md](GUIDE_CLASSIC_BASICS.ja.md)（英語版: [GUIDE_CLASSIC_BASICS.md](GUIDE_CLASSIC_BASICS.md)） |
| テスト方針・カバレッジ | [../tests/TEST_PLAN.ja.md](../tests/TEST_PLAN.ja.md) |
| 次回リリースまでに残っている作業 | [PLAN_RELEASE_NEXT.ja.md](PLAN_RELEASE_NEXT.ja.md) |
| リリース前の確認手順 | [RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md) |
| ボード / coreビルド対応表（CI生成） | `BOARDS.<version>.md` / `COMPATIBILITY.<version>.md` |
| 無印ESP32対応の方針・取得元・検証計画 | [PLAN_ESP32.ja.md](PLAN_ESP32.ja.md) |
| 無印ESP32 Classicの実装計画（配布形式・段階） | [PLAN_ESP32_CLASSIC.ja.md](PLAN_ESP32_CLASSIC.ja.md) |
| Classic Audio（A2DP / AVRCP / HFP）の拡張計画 | [PLAN_ESP32_CLASSIC_AUDIO.ja.md](PLAN_ESP32_CLASSIC_AUDIO.ja.md) |
| ESP32-P4 / ESP-Hosted対応の計画 | [PLAN_ESP_HOSTED.ja.md](PLAN_ESP_HOSTED.ja.md) |
| PCMFlowBluetoothへのA2DP検証・修正依頼 | [REQUEST_PCMFLOWBLUETOOTH_A2DP_VALIDATION.ja.md](REQUEST_PCMFLOWBLUETOOTH_A2DP_VALIDATION.ja.md) |
| 1.0.0リリース時の作業計画（履歴） | [PLAN_RELEASE_1_0_0.ja.md](PLAN_RELEASE_1_0_0.ja.md) |
| 無印ESP32でNimBLEとClassicを共存させる調査・技術検証 | [TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md) |
| 無印ESP32 Classic作業の引き継ぎ・未完了事項 | [HANDOFF_ESP32_CLASSIC.ja.md](HANDOFF_ESP32_CLASSIC.ja.md) |
| Classicの公開API・未公開機能の棚卸し | [CLASSIC_FEATURE_INVENTORY.ja.md](CLASSIC_FEATURE_INVENTORY.ja.md) |
| Classic-only Bluedroid `.a`の再生成 | [CLASSIC_HOST_BUILD.ja.md](CLASSIC_HOST_BUILD.ja.md) |
| ESP32-P4 / ESP-Hostedの準備、対応version、C6更新 | [ESP_HOSTED_SETUP.ja.md](ESP_HOSTED_SETUP.ja.md) |
| ESP32-P4 / ESP-Hostedの実機確認済み制限 | [ESP_HOSTED_LIMITATIONS.ja.md](ESP_HOSTED_LIMITATIONS.ja.md) |
| P4 Secure ConnectionsのArduino Core向けissue投稿案 | [ESP_HOSTED_SC_ISSUE.md](ESP_HOSTED_SC_ISSUE.md) |
| 使い方のサンプル | [../examples/README.ja.md](../examples/README.ja.md) |

## 文書の位置づけ

- **確定仕様（正）**: [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)、[DECISIONS.ja.md](DECISIONS.ja.md)、各SPEC。仕様の最終的な根拠。
- **現況の追跡**: [STATUS.ja.md](STATUS.ja.md)。進捗・制限・TODOを追う。まとまった作業ごとに更新。
- **提案（未確定）**: `PROPOSAL_*.ja.md`。採用が決まった項目はDECISIONSと各SPECへ移し、提案文書からは消す。
- **CI生成物（手動編集しない）**: `BOARDS.<version>.md`、`COMPATIBILITY.<version>.md`。`.github/workflows/`の`board-matrix.yml`/`core-matrix.yml`が生成する。

## 現在の一言サマリ

**BLE共通基盤と複合HID Device / Hostは公開済み**です。無印ESP32のClassicは次回リリースの対象で、機能ごとの検証状態を[棚卸し](CLASSIC_FEATURE_INVENTORY.ja.md)に明記しています。BLEとClassicの同時利用（dual-host）は実機検証済みですが実験扱いのままです。残っているのは最終回帰と外部機器との相互運用です。
