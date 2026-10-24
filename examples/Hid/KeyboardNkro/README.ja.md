# KeyboardNkro

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID編 — キーボードやマウスとして振る舞う」

固定6キーReportの代わりに29-byteのNKRO（Nキーロールオーバー）ビットマップInput Reportを使うBLE HID keyboard（HID over GATT / HOGP）です。任意個数のキーを同時に報告でき、HID Service `0x1812` をadvertiseします。キー入力はSerialコマンドで発生させます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PCやスマートフォン、または[KeyboardHost](../KeyboardHost/)を動かす2台目のボード

## 動作

- `configure()` より前に `enableNkro()` を呼び、keyboardを29-byteのNKROビットマップReportへ切り替えます
- preferred MTUに64を要求します（29-byteのNKRO Input ReportにはMTU ≥ 32が必要）
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- 切断のたびにadvertisingを再開します
- `n` で8キー同時押しを**1 report**として送信、`r` で全キーrelease

## 主なAPI

- `ble.hidKeyboard().enableNkro()` — `configure()` より前に呼ぶ必要があります
- `keyboard.configure()` — `begin()` 前にHID Serviceを構成
- `keyboard.ready()` — 購読済みHostが居て今送れるか
- `EspBleHidKeyboardNkroReport::press(usage)` — 送信するReport上で1つのHID usageを押下状態にする
- `keyboard.sendReport(nkroReport)` — NKROの全状態を1 notificationとして送信
- `keyboard.releaseAll()` — 全キーをクリア
- `config.preferredMtu = 64` — Reportに十分なMTUをネゴシエート

## メモ

- NKROビットマップReportのレイアウトはEspUsbDeviceと同じで、同一usageが同じようにマップされます。
- `keys[6]` を持つ通常の `sendReport()` はNKRO有効でも1回に6キーまでです。7キー以上を1回で送るには `EspBleHidKeyboardNkroReport` 版を使います。
- `EspBleHidKeyboardNkroReport` のbitmapが持てるのはusage `0x00`〜`0xDF` です。modifier usage `0xE0`〜`0xE7` は `press()` / `release()` が `modifiers` へ振り分けるため、呼び出し側でusageを区別する必要はありません。

## 期待されるSerial出力

```
Send 'n' for eight simultaneous keys, 'r' to release all.
```

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[KeyboardNkro.ino](KeyboardNkro.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[KeyboardHost](../KeyboardHost/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `keyboard.configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
3. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

キーボード構成前にNKROを有効にし、6キーを超える場合はNKROの全状態Reportを使います。BLEでは同時押しの確認前に、交渉後のMTUがReportに十分かも確認します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
