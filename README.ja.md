# EspBle

[English](README.md)

ESP32 Arduino向けの汎用Bluetooth Low Energyライブラリです。**Arduino-ESP32に同梱されたNimBLE backendを使用し、Bluetooth Classicには対応しません。** Central / Peripheral、GATT Client / Server、Security、標準プロファイルと独自GATTサービスを共通基盤上で扱います。外部のNimBLE-Arduinoは必須依存にしません。

現在は仕様策定とテスト基盤整備の段階です。公開APIはまだ確定していません。

Legacy Advertising、Scanning、Central/Peripheral接続と切断の最初のvertical sliceを実装し、ESP32-S3 2台の`advertise_scan`と`connect_disconnect` Peerテストで検証しています。現在のAPIと`ble.update()`によるevent配送はGATT実装前の試行段階です。

## 初期ターゲット

- Legacy AdvertisingとScanning
- Central / Peripheral接続と接続管理
- 汎用GATT Server / Client
- Read / Write / Notify / Indicate
- 基本的なSecurityとBonding
- 独自GATT Service
- HID Keyboard Central / Peripheral
- Battery Service
- エラーとログ
- ESP32-S3 2台による自動Peerテスト

Mouse、Consumer Control、複合HID、MIDI、NUS、センサー、Extended Advertisingなどは、初期基盤を検証した後に優先順位を付けて個別に追加します。

## 文書

- [要件](docs/REQUIREMENTS.ja.md)
- [コア設計](docs/CORE_DESIGN.ja.md)
- [API設計](docs/API_DESIGN.ja.md)
- [用語と命名規則](docs/TERMINOLOGY.ja.md)
- [設計決定](docs/DECISIONS.ja.md)
- [開発計画](docs/DEVELOPMENT_PLAN.ja.md)
- [テスト計画](tests/TEST_PLAN.ja.md)

`memo.ja.md`は初期検討の原文です。内容を正式文書へ移行し、移行確認が完了した時点で削除します。

## ライセンス

MIT License
