# VendorDevice

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID編 — キーボードやマウスとして振る舞う」

任意のInput / Output / Featureデータ用に、vendor定義のReport（固定Report ID 6）を公開するBLE HID device（HID over GATT / HOGP）です。HID Service `0x1812` をadvertiseし、InputはSerialコマンドで送信、Hostからの書込みは表示します。Report sizeは1〜64 bytesで構成でき、ここでは8を使います。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: [VendorHost](../VendorHost/)を動かす2台目のボード

## 動作

- `begin()` 前に `EspBleHidVendorConfig::reportSize = 8` でvendor HID profileを構成します
- preferred MTUに100を要求します
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- Hostが書き込んだOutput / Feature Reportを `onOutputReport()` / `onFeatureReport()` で表示します
- 切断のたびにadvertisingを再開します
- `i` で8byteのVendor Input Report（`'E' 'S' 'P'` ＋ 回転カウンタ ＋ `4 5 6 7`）を送信

## 主なAPI

- `ble.hidVendor().configure(vendorConfig)` — `EspBleHidVendorConfig` で `reportSize` を設定。失敗時はfalseを返す
- `vendor.sendInput(data, length)` — Vendor Input Reportを送信（成否を返す）
- `vendor.onOutputReport(cb)` / `vendor.onFeatureReport(cb)` — Hostからの書込みを `EspBleHidVendorReport`（`reportType`、`length`、`data`）で受信

## メモ

- Reportレイアウトと Report ID 6 はEspUsbDeviceのVendor HIDに揃えています。
- Output / Featureイベントは `ble.update()` から配送されます。

## 期待されるSerial出力

```
Send 'i' to send an 8-byte Vendor Input Report.
Input: sent
Output type=2 length=8 data=4f 55 54 03 04 05 06 07
```

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[VendorDevice.ino](VendorDevice.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.hidVendor().configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `ble.hidVendor().onOutputReport(...)` — HostからDeviceへのHID Output Reportを受け取ります。Report IDと長さを確認してから、キーボードLEDビットなどの項目を解釈します。
3. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
4. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

Vendor定義Reportには、相手側にも対応するアプリ／解析処理が必要です。Descriptorのバイトを変えたらReport宣言と送受信両方の処理を合わせ、機器名の見た目から形式を推測しないようにします。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
