# KeyboardDevice

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID編 — キーボードやマウスとして振る舞う」

ボードをBLE HID keyboard（HID over GATT / HOGP、固定6キーロールオーバー）にし、HID Service `0x1812` をadvertiseします。PCやスマートフォンからPairingすると実際のキーボードとして入力でき、キー入力はSerialコマンドで発生させます。Host側からのLED Output Reportも受信します。HID Boot Protocolを有効化しているためBIOS等のboot hostにも対応します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PCやスマートフォン、または[KeyboardHost](../KeyboardHost/)を動かす2台目のボード

## 動作

- `begin()` 前にHID Keyboard profileを構成します（HID + Device Information + Battery Serviceが自動合成され、HID UUIDとkeyboard appearanceがadvertisingへ追加されます）
- Boot Protocolを有効化（opt-in）し、`onProtocolMode()` を意味あるものにします。boot hostには8-byteのboot reportが自動送信されます
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- HostのLED状態（Num/Caps/Scroll Lock）を `onOutputReport()` で、現在のProtocol Modeを `onProtocolMode()` で表示します
- 切断のたびにadvertisingを再開します
- `a` でShift+Aを押し、`r` で全キーをreleaseします

## 主なAPI

- `ble.hidKeyboard()` / `keyboard.configure(config)` — `begin()` 前に構成。`EspBleHidKeyboardConfig` で `manufacturer` を設定し、`bootProtocol` を有効化
- `EspBleHidKeyboardInputReport` — `modifiers` bitmask（例: `LeftShift`）と最大6個のHID usageを持つ `keys[]`
- `keyboard.sendReport(report)` / `keyboard.releaseAll()`
- `keyboard.onOutputReport(cb)` — LED状態（`numLock`、`capsLock`、`scrollLock`）
- `keyboard.ledState()` — 変化の通知ではなく「今のLED状態」を問い合わせる
- `keyboard.onProtocolMode(cb)` — `EspBleHidKeyboard::BootProtocolMode` と比較

## メモ

- Input ReportのReport IDとreserved byteは内部で処理されます。
- Output ReportとProtocol Modeのイベントは `ble.update()` から配送されます。

## 期待されるSerial出力

```
Send 'a' to type Shift+A and 'r' to release all keys.
Protocol Mode: Report
Keyboard LEDs: num=0 caps=1 scroll=0
```

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[KeyboardDevice.ino](KeyboardDevice.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[KeyboardHost](../KeyboardHost/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

KeyboardDevice - ボードをBLE HID keyboard（HID over GATT、固定6KRO）にする。 PC/スマホからPairingすると普通のキーボードとして入力でき、キー入力はSerialコマンドで発生させる。 Host側からのLED Output Report（Num/Caps/Scroll Lock）も受信する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `keyboard.configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `keyboard.onOutputReport(...)` — HostからDeviceへのHID Output Reportを受け取ります。Report IDと長さを確認してから、キーボードLEDビットなどの項目を解釈します。
3. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
4. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
