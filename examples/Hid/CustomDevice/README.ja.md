# CustomDevice

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID編 — キーボードやマウスとして振る舞う」

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

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[CustomDevice.ino](CustomDevice.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[CustomClient](../CustomClient/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

CustomDevice - ble.hidCustom() で任意のReport DescriptorのHIDデバイスを作る。 この例はvendor定義の「コントロールパネル」: 2byteの入力Report（符号付きダイヤル差分 ＋ボタンbit）と1byteの出力Report（HostがLED状態を書き込む）。カスタムReportは 同じHID Serviceに合成されるので、ble.hidKeyboard()/hidMouse() とも共存できる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `custom.configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `custom.onOutputReport(...)` — HostからDeviceへのHID Output Reportを受け取ります。Report IDと長さを確認してから、キーボードLEDビットなどの項目を解釈します。
3. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
4. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

Vendor定義Reportには、相手側にも対応するアプリ／解析処理が必要です。Descriptorのバイトを変えたらReport宣言と送受信両方の処理を合わせ、機器名の見た目から形式を推測しないようにします。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
