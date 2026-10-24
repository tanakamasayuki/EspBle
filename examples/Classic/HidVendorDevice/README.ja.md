# HidVendorDevice（Classic）

> English: [README.md](README.md)

Report Descriptorをsketch側で書くClassic HID deviceです。usage pageはvendor用のものを
使います。keyboard、mouse、gamepad、メディアキーのどれでもない機器のための逃げ道で、
report構造はdescriptorが決め、Hostはそれに従います。**Classicは無印ESP32のみ**で動き、
届く相手はClassic HID Host——旧世代のゲーム機やPCです。BLEを話すHost向けの同等品は
[Hid/CustomDevice](../../Hid/CustomDevice/)です。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID host 1台: PC、または[HidVendorHost](../HidVendorHost/)を動かす無印ESP32

## 何をするか

- vendor usage pageで4 byteのinputをreport ID 1として宣言する
- 定期的にreportを送り、Host側で見えるようにする
- Hostの接続・切断を表示する

## 主なAPI

- `EspBleClassicHidDeviceConfig::reportDescriptor` / `reportDescriptorLength`
  — HostがSDPで読むdescriptor
- `bluetooth.hidDevice().begin(hidConfig)` — device recordを登録する
- `sendInputReport(reportId, data, length)` — descriptorに合ったreportを送る

## 補足

descriptorとdevice名などの文字列は1つのSDP recordを共有し、合計214 byteまでです。
超えるdescriptorは`begin()`が`ResourceExhausted`で拒否します——Hostから見えないdeviceを
起動しないためです。

汎用のHostはvendor usageを解釈しません。相手側も合わせて書く必要があり、
[HidVendorHost](../HidVendorHost/)はrawのreportを表示することでそれを示します。

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HidVendorDevice.ino](HidVendorDevice.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[HidVendorHost](../HidVendorHost/README.ja.md)です。2台で試す場合は相手に対応するClassicプロファイルを用意し、必要に応じて接続を受ける側を発見可能にして、開始側へそのアドレスを設定します。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.hidDevice().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
2. `bluetooth.hidDevice().onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
3. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

Vendor定義Reportには、相手側にも対応するアプリ／解析処理が必要です。Descriptorのバイトを変えたらReport宣言と送受信両方の処理を合わせ、機器名の見た目から形式を推測しないようにします。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
