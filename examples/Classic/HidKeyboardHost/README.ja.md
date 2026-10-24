# HidKeyboardHost（Classic）

> English: [README.md](README.md)

復号済みのkeyboard / mouse eventを受け取るBluetooth Classic（BR/EDR）HID hostです。
callbackとevent型はBLE host（[Hid/KeyboardHost](../../Hid/KeyboardHost/)）と同じで、
違うのは無線と相手の指定方法だけです。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台（HID host）
- Classic HID device 1台: keyboard、または[HidKeyboard](../HidKeyboard/)や
  [HidGamepad](../HidGamepad/)を動かす無印ESP32

`keyboardAddress`に相手のaddressを入れます。[Inquiry](../Inquiry/)で探せます。

## 動作

- addressを指定して接続する。Classicには絞り込むadvertisementが無いため
- SDPで受け取ったReport Descriptorからkeyboard / mouse reportを復号する。独自の
  layoutを持つdeviceでもeventとして届く
- 同じreportのusage単位eventより先にkeyboard stateを配送する（BLE hostと同順）
- 分類できないreportは`onInputReport()`へraw（payloadの前にreport ID）で渡す
- LEDのreport IDは仮定せず相手のdescriptorから取る

## 主なAPI

- `bluetooth.hidHost().connect(address)`
- `onKeyboardState()` / `onKeyboard()` / `onMouse()` / `onInputReport()`
- `setKeyboardLayout()` — usageをどの文字と解釈するかはこちら側のlayoutが決める。
  usageの選択はdeviceが自分のlayoutで行っている
- `setKeyboardLeds(numLock, capsLock, scrollLock)`

## Serialコマンド

| キー | 動作 |
|---|---|
| `c` | Caps Lock LED点灯 |
| `0` | 全LED消灯 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HidKeyboardHost.ino](HidKeyboardHost.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[HidKeyboard](../HidKeyboard/README.ja.md)です。2台で試す場合は相手に対応するClassicプロファイルを用意し、必要に応じて接続を受ける側を発見可能にして、開始側へそのアドレスを設定します。

## この例の構成

復号済みのkeyboard / mouse eventを受け取るClassic HID Host。callbackと event型はBLE host（examples/Hid/KeyboardHost）と同じで、違うのは無線と 相手の指定方法だけ。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `keyboard.onInputReport(...)` — Descriptor・Report ID・長さに従って受信HID Reportを確認します。独自データが自動的にキーボードイベントになるわけではありません。
2. `keyboard.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `keyboard.onConnectionFailed(...)` — 接続要求が受理されたあとに発生する、相手不在や接続拒否などの失敗を確認します。
4. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
5. `bluetooth.hidHost().connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

対応HID Deviceを使い、無線接続だけでなくプロファイル探索／準備完了まで待ちます。IDとDescriptorに応じてReportを振り分けます。キーボードLEDなどの出力はHost→Device方向で、キー入力の受信とは別です。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
