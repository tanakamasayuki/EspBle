# Inquiry（Classic）

> English: [README.md](README.md)

Bluetooth Classicのdevice discoveryです。他のClassic profileはaddressを指定して接続する
ため、addressを知らない場合の入手経路がここになります。**Classicは無印ESP32のみ**で動き、
見つかるのはClassic機器だけです——BLEしか持たないperipheralはinquiryに応答せず、BLEのscan
（`EspBle::scanner()`）ではClassic専用機器は見つかりません。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- 発見可能なClassic機器: Bluetooth設定画面を開いた電話機、pairing modeのheadset、または
  [SppServer](../SppServer/)を動かす無印ESP32

## 何をするか

- 約10秒間のinquiryを1回実行し、結果を表示する
- name、Class of Device、RSSIは応答に含まれていたときだけ表示する。inquiry結果に名前が
  入らないことは珍しくない
- 終了は別途通知し、`stop()`による中断と時間切れを区別する

## 主なAPI

- `bluetooth.inquiry().start(config)` — 探索開始。`true`は開始したという意味で、完了では
  ない
- `onResult()` — 応答1件につき1回。同じpeerが2回応答すれば2回届くので、必要ならaddressで
  重複を除く
- `onComplete()` — `cancelled`で`stop()`と時間切れを区別する
- `bluetooth.inquiry().requestName(address)` / `requestServices(address)` — 相手へ直接
  問い合わせる。名前が無い場合やservice一覧が欲しい場合に使う。どちらもinquiry実行中は
  応答が来ない

## 補足

`durationSeconds`は切り上げられます。controllerは1.28 s単位で数えるため、10は10.24 sに
なります。

接続できる機器が必ずinquiryで見つかるとは限りません。`ConnectableOnly`の相手は結果に
出ませんが、addressを知っている側からは接続できます。

## 関連するガイド

- [Classic入門ガイド §3 Inquiry](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#3-inquiry) — scanが返すもの、addressを取る照会

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[Inquiry.ino](Inquiry.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

Classicのdevice discovery。他のClassic profileはaddress指定で接続するため、 addressを知らない場合の入手経路がここになる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `bluetooth.inquiry().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。

## 確認と変更のポイント

Classic Inquiryで探すのはClassicの相手です。BLE専用機器が見つからなくても探索失敗とは限りません。相手を発見可能にして、対応するClassic Client例に使うアドレスを記録します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
