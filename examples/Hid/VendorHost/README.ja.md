# VendorHost

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID編 — キーボードやマウスとして振る舞う」

vendor定義のHIDデバイス向けBLE HID Host（Central）です。HID Service `0x1812` をscanし、Pairing・Discovery後にVendor Input Reportを受信し、Vendor Output / Feature Reportを書き込みます。security完了後の `discover()` フローはほかのHID種別とすべて同じです。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Host / Central）
- HIDデバイス × 1: [VendorDevice](../VendorDevice/)を動かす2台目のボード

## 動作

- HID Service `1812` をadvertiseする最初の機器をscanして接続します
- preferred MTUに100を要求し、Bondingつきでsecurityを有効化します
- 暗号化成功後に `onSecurityChanged` からHID Discoveryを開始します
- 受信したVendor Input Reportを `onVendorInput()` で表示します（Report ID、length、raw bytes）
- `o` で8byteのVendor Output Report、`f` で8byteのVendor Feature Reportを書き込み（接続中のみ）

## 主なAPI

- `ble.hidHost().discover(connectionId)` — security完了後にHID Discoveryを開始
- `hidHost().onDiscovered(cb)` — `success` / `detail` を持つ `EspBleHidKeyboardHostDiscovery`
- `hidHost().onVendorInput(cb)` — `reportId`、`rawLength`、`rawData` を持つ `EspBleHidVendorInputEvent`
- `hidHost().sendVendorOutput(connectionId, data, length)` / `sendVendorFeature(connectionId, data, length)` — デバイスへ書込み（成否を返す）

## 期待されるSerial出力

```
Send 'o' for Output or 'f' for Feature after discovery.
HID discovery: ready
Vendor Input report=6 length=8 data=45 53 50 00 04 05 06 07
Output: sent
```

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[VendorHost.ino](VendorHost.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.onSecurityChanged(...)` — セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
4. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
5. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
6. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

対応HID Deviceを使い、無線接続だけでなくプロファイル探索／準備完了まで待ちます。IDとDescriptorに応じてReportを振り分けます。キーボードLEDなどの出力はHost→Device方向で、キー入力の受信とは別です。

Vendor定義Reportには、相手側にも対応するアプリ／解析処理が必要です。Descriptorのバイトを変えたらReport宣言と送受信両方の処理を合わせ、機器名の見た目から形式を推測しないようにします。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
