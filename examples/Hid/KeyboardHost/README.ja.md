# KeyboardHost

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID編 — キーボードやマウスとして振る舞う」

HID Host（Central）としてBLE keyboardへ接続します。HID Service `0x1812` をscanし、Pairing後にHID ReportをDiscoveryしてキーイベントを表示します。単一の `hidHost()` オブジェクトがmouse / consumer control / system control / gamepad Reportも配送するため、市販のBLE keyboardでも、2台目のボードで動かす[KeyboardDevice](../KeyboardDevice/)・[KeyboardNkro](../KeyboardNkro/)・[CompositeKeyboardMouse](../CompositeKeyboardMouse/) exampleでも使えます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Host / Central）
- BLE HID機器 × 1: 市販keyboard、またはKeyboardDevice / KeyboardNkro / CompositeKeyboardMouseを動かす2台目のボード

## 動作

- HID Service `1812` をadvertiseする最初のconnectableな機器をscanして接続します
- Bondingつきでsecurityを有効化し、HID Discoveryは`onSecurityChanged`の成功**後**に開始します — 市販keyboardは暗号化前のHID属性アクセスを拒否するためです
- layoutをEN-USに設定し、Discovery結果（Report ID、Battery）、raw usage snapshot、layout変換したASCII値つきのキーpressイベントを表示します
- 同じ `hidHost()` からmouse、consumer control、system control、gamepadのイベントも表示します
- `c` でCaps Lock LEDを点灯、`0` で全LED消灯（接続中のみ）

## 主なAPI

- `ble.hidHost().discover(connectionId)` — 明示的なHID Discovery。再接続のたびに新しいConnection IDで呼び直します
- `keyboard.onDiscovered(cb)` — `success`、`reportId`、`hasBatteryLevel`、`batteryLevel`、`detail` を持つ `EspBleHidKeyboardHostDiscovery`
- `keyboard.onKeyboardState(cb)` — layout非依存の256-bit usage snapshot（`isDown()`、`wasPressed()`、`wasReleased()`、`modifiers`）
- `keyboard.setKeyboardLayout(EspBleKeyboardLayout::EnUs)` / `keyboard.onKeyboard(cb)` — usage単位のpress/releaseイベント。`ascii` は変換可能な場合のみ非0
- `keyboard.onMouse()` / `onConsumerControl()` / `onSystemControl()` / `onGamepad()` — 複合HIDの種別別イベント
- `keyboard.setKeyboardLeds(connectionId, num, caps, scroll)` — fire-and-forgetのLED書込み（Write Without Response）

## メモ

- Discovery / state / keyイベントはすべて `ble.update()` から配送されます。

## 期待されるSerial出力

```
Keyboard ready: report=1 battery=73%
Keyboard state: modifiers=0x00 A=1 pressed=1 released=0
Key pressed: usage=0x04 ascii=0x61
```

同じreportについてはstate callbackが先に呼ばれ、そのあとusage単位のkey eventが届きます。
`ascii`はそのreportのmodifiersに従うので、同じキーでもShiftなしなら`0x61`、Shiftありなら
`0x41`になります。

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[KeyboardHost.ino](KeyboardHost.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[KeyboardDevice](../KeyboardDevice/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

KeyboardHost - HID HostとしてBLE keyboardへ接続する。HID Serviceをscan → Pairing → Discovery → キーイベント表示。市販BLE keyboardでも KeyboardDevice example でも使える。 keyboard LEDの書込みも行う。

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

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
