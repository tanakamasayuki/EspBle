# HidComposite（Classic）

> English: [README.md](README.md)

keyboard、mouse、メディアキーを兼ねる1台のBluetooth Classic（BR/EDR）HID deviceです。
Classicはdevice recordを1つ登録するため、すべてが1つの合成Report Descriptorに入り、
profileごとにreport IDが分かれます。

**合成できるprofile数にはBLEには無い上限があります。**Report Descriptorとprofileの
文字列3つは1つのSDP recordを共有し、合計214 byteまでです。この3つでdescriptorは
144 byte、文字列が57 byteなので201 byteで収まります。gamepadを加えるとdescriptorが
212 byteになり、何も登録されません。`begin()`はそうした組み合わせを拒否します——誰も到達できない
deviceを起動しないためです。gamepadは[HidGamepad](../HidGamepad/)を単独で使ってください。BLE側の
[Hid/CompositeKeyboardMouse](../../Hid/CompositeKeyboardMouse/)と対になります。
**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす無印ESP32

## 動作

- `begin()`前に3つのprofileを設定する。設定した内容がdescriptorを決めるため、後から
  追加してもHostが読み終えたrecordには入らない
- Class of Deviceは1つ選ぶ。複合deviceでも、Hostが提示する姿は1つに決まる
- Serialコマンドでprofileごとに1 reportを送る

## 主なAPI

- `hidKeyboard()` / `hidMouse()` / `hidConsumerControl()` — まとめて設定し、個別に送る
- 送信はそれぞれのreport IDで出るため、keyboardしか解釈しないHostは他を無視する

## Serialコマンド

| キー | 動作 |
|---|---|
| `k` | "hi"と入力 |
| `m` | pointerを移動 |
| `v` | 音量を上げる |
| `r` | keyboardとmouseを解放 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HidComposite.ino](HidComposite.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

keyboard、mouse、メディアキーを兼ねる1台のBluetooth Classic（BR/EDR） HID device。Classicはdevice recordを1つ登録するため、すべてが1つの合成 Report Descriptorに入り、profileごとにreport IDが分かれる。 BLE側のexamples/Hid/CompositeKeyboardMouseと対になる。  合成できるprofile数にはBLEには無い上限がある。Report Descriptorとprofileの 文字列3つは1つのSDP recordを共有し、合計214 byteまで。この3つでdescriptorは 144 byte、下の文字列が57 byteなので201 byteで収まる。gamepadを加えると descriptorが212 byteになり何も登録されない——単独の例は examples/Classic/HidGamepadにある。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.hidKeyboard().configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `bluetooth.hidKeyboard().onOutputReport(...)` — HostからDeviceへのHID Output Reportを受け取ります。Report IDと長さを確認してから、キーボードLEDビットなどの項目を解釈します。
3. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
4. `bluetooth.hidDevice().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
