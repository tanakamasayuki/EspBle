# CustomDevice

> English: [README.md](README.md)

`ble.hidCustom()` で**任意のReport Descriptor**を持つHIDデバイスを作ります。固定profile（`hidKeyboard()`、`hidMouse()` …）と違い、raw HID Report Mapを自分で与えて各Reportを宣言するので、任意のデバイス形状を表現できます。カスタムReportは同じHID Serviceに合成されるため、内蔵profileと共存できます。

この例はvendor定義の「コントロールパネル」です。Report ID 1の下に、2byteの入力Report（符号付きダイヤル差分＋ボタンbit）と1byteの出力Report（HostがLED状態を書き込む）を持ちます。

2枚目のボードで [Hid/CustomClient](../CustomClient/) を動かすか、任意のGATTクライアント（nRF Connect等）でReport Mapを読み、入力Reportを購読してください。

## ハードウェア

- ESP32-S3 × 1（このスケッチ。HIDデバイス / peripheral）
- HID Host × 1（Hid/CustomClientを動かす2枚目のボード、またはGATTクライアントアプリ）

## 動作

- `begin()` 前に `ble.hidCustom().configure()` → `setReportMap()`（raw descriptor）→ `addInputReport(1, 2)` / `addOutputReport(1, 1)` を呼ぶ
- HID Service（`0x1812`）をadvertise
- Serialで `i` を送ると2byteの入力Report（ダイヤル差分 `+5`、ボタン `0x01`）を送信
- Hostが1byteの出力Reportを書き込むたびに `onOutputReport()` で内容を表示

## API補足

- `configure()` を最初に呼び、その後 `setReportMap()` / `addInputReport()` / `addOutputReport()` / `addFeatureReport()`、すべて `begin()` より前に行う
- Report IDは一意にする。内蔵profileを併用する場合はその予約ID（1〜6）を避ける
- Reportサイズはbyte単位（1〜64）。`sendInput(reportId, data, length)` は宣言サイズと一致させる
- 実機HostはHIDに暗号化linkを要求するのが通常なので、この例ではsecurityを有効化している
