# HidKeyboardNkro（Classic）

> English: [README.md](README.md)

N-key rollover対応のBluetooth Classic（BR/EDR）keyboardです。各キーがreport内の
1 bitなので6キー制限がありません。BLE側の[Hid/KeyboardNkro](../../Hid/KeyboardNkro/)と
対になります。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす無印ESP32

## 動作

- `enableNkro(true)`を`configure()`より**前に**呼ぶ。NKROはReport Descriptorを変え、
  Hostはpairing時にそれを一度だけ読むため
- 8キー同時押しを送る（6キーのreportでは表現できない）
- 文字入力は6KROと同じ便利APIで行う

## 主なAPI

- `bluetooth.hidKeyboard().enableNkro(true)` — `configure()`より前に呼ぶ
- `pressUsage(usage)` — 押下状態へ追加し、状態全体が1 reportで送られる
- `write("...")` — descriptorの選択によらず同じ

## Serialコマンド

| キー | 動作 |
|---|---|
| `8` | 8キー同時押し |
| `w` | "nkro"と入力 |
| `r` | すべて解放 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HidKeyboardNkro.ino](HidKeyboardNkro.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

N-key rollover対応のBluetooth Classic（BR/EDR）keyboard。各キーが1 bitなので 6キー制限が無い。呼び出しはBLE example（examples/Hid/KeyboardNkro）と同じ。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.hidKeyboard().configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
3. `bluetooth.hidKeyboard().onOutputReport(...)` — HostからDeviceへのHID Output Reportを受け取ります。Report IDと長さを確認してから、キーボードLEDビットなどの項目を解釈します。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

キーボード構成前にNKROを有効にし、6キーを超える場合はNKROの全状態Reportを使います。BLEでは同時押しの確認前に、交渉後のMTUがReportに十分かも確認します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
