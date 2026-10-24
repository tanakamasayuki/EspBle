# Gamepad

> English: [README.md](README.md)
> 概念: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID」

GATT（HOGP）上のBLE HID gamepadです。符号付き6軸、hat switch、32 buttonを1つの
Input Reportで送ります。Classic側にも同じ呼び出しがあり
（[Classic/HidGamepad](../../Classic/HidGamepad/)）、旧世代のゲーム機やPCには
そちらが必要です（BR/EDR HIDしか受け付けないため）。

## 必要なもの

- このsketchを動かすESP32-S3 1台（HID device / peripheral）
- HID host 1台: PCや携帯、または[KeyboardHost](../KeyboardHost/)を動かす2台目
  （`onGamepad()`がeventを表示する）

## 動作

- `begin()`前に`ble.hidGamepad().configure()`でHID Serviceへ合成する
- bonding付きsecurityを有効にする（HOGPは暗号化linkを要求する）
- 切断ごとにadvertisingを再開する
- Serialコマンドでbutton、stick位置、hat方向を送る

## 主なAPI

- `ble.hidGamepad().configure()` — HID Serviceへgamepadを合成する
- `send(x, y, z, rz, rx, ry, hat, buttons)` — 軸の値から1 reportを送る
- `sendReport(report)` — `EspBleHidGamepadReport`を埋めて送る同等の呼び出し
- `releaseAll()` — 全軸中央・全button解放。Hostは送った最後のreportを保持し続けるため、
  入力を止めるにはこれを送る

## Serialコマンド

| キー | 動作 |
|---|---|
| `a` | button 1 |
| `b` | button 2 |
| `d` | 左stickを右上、hatは上、button 1 |
| `r` | すべて解放 |

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Gamepad.ino](Gamepad.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

GATT（HOGP）上のBLE HID gamepad。符号付き6軸、hat switch、32 button。 Classic側にも同じ呼び出しがある（examples/Classic/HidGamepad）。BR/EDR HIDしか 受け付けない相手にはそちらを使う。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.hidGamepad().configure(...)` — スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
2. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
3. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。

## 確認と変更のポイント

対応Hostとペアリングし、影響のないテスト用ウィンドウを選んでから、上記のシリアル操作を試します。キーやボタンの押下は状態を持つので、保持した入力は明示的に解除し、Report ID・データ長・Descriptorを一致させます。形式を変えた場合はHostが旧Descriptorを保持することがあるため、登録を削除して再接続して確認します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
