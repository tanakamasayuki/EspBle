# HidMouse（Classic）

> English: [README.md](README.md)
> どちらの無線を使うか: [BLEとClassic](../../../docs/CLASSIC_VS_BLE.ja.md)

Bluetooth Classic（BR/EDR）のHID mouseです。呼び出しはBLE版の
[Hid/Mouse](../../Hid/Mouse/)と同じで、違うのは無線だけです。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台（HID device）
- Classic HID host 1台: PC、または[HidKeyboardHost](../HidKeyboardHost/)を動かす
  無印ESP32（`onMouse()`が復号したeventを表示する）

## 動作

- `begin()`前にmouse profileを設定する
- Peripheral / pointing deviceのClass of Deviceを宣言し、Hostが未分類ではなく
  mouseとして扱うようにする
- Serialコマンドで移動、クリック、スクロール、ドラッグを行う

## 主なAPI

- `bluetooth.hidMouse().configure()` — `begin()`前にmouseを加える
- `move(dx, dy)` — 相対・符号付きの移動量
- `click(button)` — 押下と解放を1回の呼び出しで送る
- `wheel(delta)` — pointerを動かさず、押しているbuttonも保持したままスクロール
- `press(button)` / `releaseAll()` — 押下は加算される（ドラッグに必要）。
  `buttons()`で現在押している状態を読む

## Serialコマンド

| キー | 動作 |
|---|---|
| `m` | (+12, -8)移動 |
| `c` | 左クリック |
| `w` | 1段スクロール |
| `p` | 左を押しながら移動（ドラッグ） |
| `r` | すべてのbuttonを解放 |

## 関連するガイド

- [Classic入門ガイド §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#6-hid) — SDP record、214 byteの予算、Hostが復号する範囲
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — Classicでのreport IDの位置と確かめ方
- [BLEとClassicの選び方](../../../docs/CLASSIC_VS_BLE.ja.md) — Classic HIDとBLE HIDの違い

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[HidMouse.ino](HidMouse.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

Bluetooth Classic（BR/EDR）のHID mouse。呼び出しはBLE example （examples/Hid/Mouse）と同じで、違うのは無線だけ。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.hidMouse().configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
3. `bluetooth.hidDevice().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
