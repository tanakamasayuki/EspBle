# HidVendorHost（Classic）

> English: [README.md](README.md)

標準では説明されないreportを送るClassic HID deviceのHost側です。address指定で接続し、
input reportをbyte列として表示します。vendor固有deviceに必要なのはこれで、Host側の
復号はkeyboardとmouseしか扱わず、それ以外はrawで届きます。**Classicは無印ESP32のみ**で
動き、接続先はClassic HID deviceです。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- Classic HID device 1台: [HidVendorDevice](../HidVendorDevice/)を動かす無印ESP32、
  または任意のBR/EDR HID機器

## 何をするか

- serial consoleへ入力したaddressへ接続する
- input reportをreport IDとbyte列で表示する
- 受理された後に失敗した接続を通知する

## 主なAPI

- `bluetooth.hidHost().connect(address)` — Classicには絞り込むadvertisementが無いため
  addressが必要
- `onInputReport()` — deviceが送ったままのreport
- `onConnectionFailed()` — 後から非同期に失敗したことを受け取る

## 補足

**Classic HID Hostが復号するのはkeyboardとmouseだけです。**consumer / system / gamepad /
vendorのreportは`onInputReport()`へrawで届くため、この例はbyte列を表示します。BLE側の
hostはより多くを復号します——[BLEとClassic](../../../docs/CLASSIC_VS_BLE.ja.md)を参照して
ください。

report IDを宣言するdeviceのreportは、payload先頭にIDが入ります。そのため`value`と
`reportId`は同じことを2度示します。

HID Hostの接続は同時に1つです。

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HidVendorHost.ino](HidVendorHost.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[HidVendorDevice](../HidVendorDevice/README.ja.md)です。2台で試す場合は相手に対応するClassicプロファイルを用意し、必要に応じて接続を受ける側を発見可能にして、開始側へそのアドレスを設定します。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.hidHost().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
2. `bluetooth.hidHost().onConnectionFailed(...)` — 接続要求が受理されたあとに発生する、相手不在や接続拒否などの失敗を確認します。
3. `bluetooth.hidHost().onInputReport(...)` — Descriptor・Report ID・長さに従って受信HID Reportを確認します。独自データが自動的にキーボードイベントになるわけではありません。
4. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
5. `bluetooth.hidHost().connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

対応HID Deviceを使い、無線接続だけでなくプロファイル探索／準備完了まで待ちます。IDとDescriptorに応じてReportを振り分けます。キーボードLEDなどの出力はHost→Device方向で、キー入力の受信とは別です。

Vendor定義Reportには、相手側にも対応するアプリ／解析処理が必要です。Descriptorのバイトを変えたらReport宣言と送受信両方の処理を合わせ、機器名の見た目から形式を推測しないようにします。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
