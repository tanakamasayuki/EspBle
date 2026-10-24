# AvrcpController（Classic）

> English: [README.md](README.md)

AVRCP Controllerです。相手側の再生・停止を操作する側になります。Target側（押された
操作を受ける側）は[A2dpSinkAvrcp](../A2dpSinkAvrcp/)にあります。AVRCPは操作だけを運び、
音声はA2DPが運びます。AVRCPの接続はA2DP接続に追従します。
**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- 再生側 1台: 携帯、または[A2dpSinkAvrcp](../A2dpSinkAvrcp/)を動かす無印ESP32

`playerAddress`に相手のaddressを入れます。[Inquiry](../Inquiry/)で探せます。

## 動作

- backendの要件によりAVRCPをA2DPより先に開始する
- Controller roleだけを動かす。両方を動かすこともでき、A2dpSinkAvrcpがその例
- 再生操作キーを送り、相手が受理したかを表示する。`accepted=0`はcommandを理解した上で
  拒否したという意味で、届かなかった場合とは別である
- play statusとmetadataを要求し、absolute volumeとplayer settingを設定する

## 主なAPI

- `bluetooth.avrcp().sendKey(command)` / `onPassthroughResponse()`
- `requestPlayStatus()` / `onPlayStatus()`
- `requestMetadata(mask)` / `onMetadata()`
- `setAbsoluteVolume(0〜127)` — パーセントではない
- `setPlayerSetting(attribute, value)` — repeatやshuffleなど

## Serialコマンド

| キー | 動作 |
|---|---|
| `c` | mediaを接続し、AVRCPもそれに追従する |
| `p` / `x` | 再生 / 一時停止 |
| `n` / `b` | 次 / 前の曲 |
| `i` | play statusを要求 |
| `m` | title・artist・albumを要求 |
| `v` | absolute volumeを64にする |
| `r` | 1曲リピートにする |

## 関連するガイド

- [Classic入門ガイド §7 A2DPとAVRCP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#7-a2dpとavrcp) — encode済みmedia、codec設定、操作

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[AvrcpController.ino](AvrcpController.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

AVRCP Controller。相手側の再生・停止を操作する側。Target側（押された操作を 受ける側）はexamples/Classic/A2dpSinkAvrcpにある。AVRCPは操作だけを運び、 音声はA2DPが運ぶ。AVRCPの接続はA2DP接続に追従する。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `bluetooth.a2dpSource().connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

スピーカーのアドレスを置き換え、必要なプロファイルの接続後に操作します。メタデータ／制御応答と音声経路は別で、コマンド送信の受理だけでは音が出ることは確認できません。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
