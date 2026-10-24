# HidKeyboard（Classic）

> English: [README.md](README.md)
> どちらの無線を使うか: [BLEとClassic](../../../docs/CLASSIC_VS_BLE.ja.md)

Bluetooth Classic（BR/EDR）のkeyboardとmouseです。reportとdescriptorを共有している
ため、呼び出しはBLE版の[Hid/KeyboardDevice](../../Hid/KeyboardDevice/)と同じで、
違うのは無線だけです。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台（HID device）
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす無印ESP32

## 動作

- keyboardとmouseのprofileを`begin()`より前に設定する。合成したReport Descriptorは、
  Hostがpairing時に読むdevice recordの一部になるため
- Hostが返すLED状態を表示する
- GPIO 0をGNDへ落とすボタンの押下を検出し、準備完了時に `hello` の文字入力とマウス移動を行う。`INPUT_PULLUP` なので未押下はHIGH

## 主なAPI

- `bluetooth.hidKeyboard().configure()` / `bluetooth.hidMouse().configure()`
- `ready()` — Hostが入力Reportを受け取れる状態か確認する
- `write("hello")` / `bluetooth.hidMouse().move(10, 0)` — ボタンを押した瞬間に文字と相対移動を送る
- `onOutputReport()` — HostのCaps Lock・Num Lock状態

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HidKeyboard.ino](HidKeyboard.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[HidKeyboardHost](../HidKeyboardHost/README.ja.md)です。2台で試す場合は相手に対応するClassicプロファイルを用意し、必要に応じて接続を受ける側を発見可能にして、開始側へそのアドレスを設定します。

## この例の構成

ClassicのHID keyboard / mouse。BLE example（examples/Hid/KeyboardDevice）と 同じ呼び出しになる。reportとdescriptorを共有しており、違うのは無線だけ。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.hidKeyboard().configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `bluetooth.hidKeyboard().onOutputReport(...)` — HostからDeviceへのHID Output Reportを受け取ります。Report IDと長さを確認してから、キーボードLEDビットなどの項目を解釈します。
3. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、GPIO 0のボタン操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)

GPIO 0がボード上のBOOTボタンと共用される場合は、書き込み・起動時の扱いをボードの資料で確認します。この例は押下エッジを検出しますが、機械接点のデバウンス処理は含みません。実際のボタン入力へ組み込む場合は必要に応じ追加してください。
