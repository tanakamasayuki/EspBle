# CustomDevice

> English: [README.md](README.md)

`ble.hidCustom()` で**任意のReport Descriptor**を持つBLE HID device（HID over GATT / HOGP）を作ります。raw HID Report Mapを自分で与えて各Reportを宣言するので任意のデバイス形状を表現でき、カスタムReportは同じHID Service（`0x1812`）に合成されて `hidKeyboard()`/`hidMouse()` とも共存できます。この例はvendor定義の「コントロールパネル」（vendor usage page `0xFF00`、Report ID 1）で、2byteの入力Report（符号付きダイヤル差分＋ボタンbit）と1byteの出力Report（HostがLED状態を書き込む）を持ちます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: [CustomClient](../CustomClient/)を動かす2台目のボード、またはGATTクライアントアプリ（nRF Connect等）

## 動作

- `begin()` 前に `configure()` → `addInputReport(1, 2)` / `addOutputReport(1, 1)` → `setReportMap()`（raw descriptor）を呼びます
- HID Service（`0x1812`）をadvertiseします
- Bondingつきでsecurityを有効化します — 実機HostはHIDに暗号化linkを要求します
- Hostが1byteの出力Reportを書き込むたびに `onOutputReport()` で内容を表示します
- 切断のたびにadvertisingを再開します
- `i` で2byteの入力Report（ダイヤル差分 `+5`、ボタン `0x01`）を送信

## 主なAPI

- `ble.hidCustom().configure()` — ほかの `hidCustom()` 設定より前に呼び、すべて `begin()` より前に行う
- `custom.addInputReport(id, bytes)` / `addOutputReport(id, bytes)`（`addFeatureReport` も）— 各Reportのidとbyte数（1〜64）を宣言
- `custom.setReportMap(descriptor, size)` — raw HID Report Mapを設定
- `custom.sendInput(reportId, data, length)` — 宣言サイズと一致する入力Reportを送信
- `custom.onOutputReport(cb)` — Hostからの書込みを `EspBleHidVendorReport`（`reportId`、`length`、`data`）で受信

## メモ

- Report IDは一意にする。Report ID 1〜6は、対応する内蔵profileを有効にしている場合のみ予約されます。
- Output / Featureイベントは `ble.update()` から配送されます。

## 期待されるSerial出力

```
Send 'i' to send an input report (dial +5, button 1).
Output report id=1 len=1 value=2
```
