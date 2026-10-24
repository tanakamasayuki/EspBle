# RadioSettings（Classic）

> English: [README.md](README.md)

Classicのsketchが選べる無線・linkの設定3つ——送信電力、page timeout、暗号鍵の最小長
——を扱います。どれもprofileの動作を変えないため、接続試行を通して見せます。page
timeoutを短くすると、居ない相手に対して`connect()`が諦めるまでの時間が短くなります。
**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台

相手は不要です。`absentAddress`は誰も応答しないaddressで、それがpage timeoutを
見えるようにします。実在のaddressに変えれば接続が成功する側を見られます。

## 何をするか

- BR/EDRの送信電力を範囲で設定する。電力制御はその範囲の中からpacketごとにlevelを
  選ぶためである。1つだけ渡す形は上下限を同じ値に固定する
- 接続の前にpage timeoutを1000 msへ短くする。居ない相手への試行が既定の5120 msでは
  なく約1秒で終わる
- 16 byteより短い暗号鍵を拒否する
- 無線が適用した値——対応するlevelへ丸めた後の値——を表示する

## 主なAPI

- `bluetooth.setTxPower(minimum, maximum)` / `setTxPower(dBm)` — -12〜+9 dBmの3 dB刻み。
  BLEの送信電力を設定する`EspBle::setTxPower()`とは別物
- `bluetooth.txPower(minimum, maximum)` — 無線が適用した範囲
- `bluetooth.setPageTimeout(milliseconds)` — 14〜40959 ms、既定5120 ms。次のpageから効く
- `bluetooth.pageTimeout()` — 確定した値。確定するまでは0
- `bluetooth.setMinimumEncryptionKeySize(bytes)` — 7〜16。以降に確立するlinkに効く

## Serialコマンド

| キー | 動作 |
|---|---|
| `p` | 送信電力の範囲とpage timeoutを表示 |
| `c` | `absentAddress`へ接続を試み、失敗までの時間を測る |

## 補足

`setPageTimeout()`の`true`は要求が受理されたという意味です。backendは自分のtaskで
確定させるため、確定が届くまで`pageTimeout()`は0を返します。controllerの既定値を知る
ためにlibraryが起動時に出す照会も同じです。

page timeoutを短くすると応答が遅れただけの相手を諦め、送信電力を下げると距離が
縮みます。どちらも改善ではなくtrade-offです。

## 関連するガイド

- [Classic入門ガイド §2 起動と可視性](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#2-起動と可視性) — 送信電力、page timeout、可視性

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[RadioSettings.ino](RadioSettings.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

Classicのsketchが選べる無線・linkの設定3つ: 送信電力、page timeout、 暗号鍵の最小長。どれもprofileの動作を変えないため、接続試行で見るのが 一番分かりやすい。page timeoutを短くすると、居ない相手に対してconnect()が 諦めるまでの時間が短くなる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `bluetooth.spp().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `bluetooth.spp().onConnectionFailed(...)` — 接続要求が受理されたあとに発生する、相手不在や接続拒否などの失敗を確認します。
4. `bluetooth.spp().connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

無線／セキュリティ設定を1つずつ変えて結果を比べます。送信電力やPage Timeoutを増やしても存在しないサービスは追加できず、暗号鍵の条件によっては発見できる相手も拒否されます。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
