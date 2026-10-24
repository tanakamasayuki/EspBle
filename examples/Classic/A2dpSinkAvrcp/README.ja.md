# A2dpSinkAvrcp（Classic）

> English: [README.md](README.md)

A2DP SinkとAVRCPの組み合わせです。speakerやheadsetに必要な対で、A2DPが音楽、AVRCPが
ボタンと音量を運びます。**Classicは無印ESP32のみ**で動き、どちらのprofileも届く相手は
Classic機器——電話機、tablet、PCです。この側はTargetで、電話機が再生を押すとこの基板が
それを知ります。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- AVRCPを持つA2DP source 1台: 電話機・tablet・PC

## 何をするか

- backendの要求どおり、A2DPより先にAVRCPを開始する
- sourceが送るpassthrough keyを、押下と解放で分けて表示する
- 音量変化を表示し、remoteからの指示か自分側の変化かを区別する
- A2DPの接続を表示する

## 主なAPI

- `bluetooth.avrcp().begin()` — `a2dpSink().begin()`より**前に**呼ぶ
- `onPassthrough()` — 再生・停止・次曲などをkeyの押下／解放として受け取る
- `onVolumeChanged()` — `remoteCommand`で指示と報告を区別する

## 補足

**AVRCPはA2DPより先に開始します。**逆順にすると、sourceがaudio profileを確立する時点で
制御チャネルが使えません。

**このbuildでTargetが宣言できるnotificationは音量変化だけです。**同梱Classic hostが他を
許さず、`supportedNotifications()`が許可集合を返し、許可外の宣言は理由付きで拒否されます。
Targetとしてmetadataやplay statusを送るAPIはbackendに存在しません。この機器自身の音量を
sourceへ報告するには`bluetooth.avrcp().setAbsoluteVolume(value)`を呼びます——このsketchは
音量指示を受けるだけで、自分からの報告は行いません。

keyを受けるのではなく送る側になるなら、この機器はControllerです——
[AvrcpController](../AvrcpController/)を参照してください。

## 関連するガイド

- [Classic入門ガイド §7 A2DPとAVRCP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#7-a2dpとavrcp) — encode済みmedia、codec設定、操作

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[A2dpSinkAvrcp.ino](A2dpSinkAvrcp.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.a2dpSink().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
2. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。

## 確認と変更のポイント

音声相手を迎える前に制御経路を用意するため、Sinkより前にAVRCPを開始します。AVRCPのキーや音量イベントは再生方針の制御で、A2DP音声のデコードや出力ではありません。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
