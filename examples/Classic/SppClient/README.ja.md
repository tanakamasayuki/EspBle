# SppClient（Classic）

> English: [README.md](README.md)

SPPの接続する側です。serverは待って見つけられる側ですが、clientはaddressを知り、
その先のRFCOMM channelを解決する必要があります。[SppServer](../SppServer/)では
示せない部分がここにあります。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台（client）
- SPP server 1台: [SppServer](../SppServer/)を動かす無印ESP32、またはserial serviceを
  公開するPCやAndroid機器

`serverAddress`にserverのaddressを入れます。[Inquiry](../Inquiry/)で探せます。

## 動作

- `connect(address)`はSDPで相手のSPP serviceのchannelを問い合わせる
- `connectToChannel(address, channel)`は既に分かっているchannelへdiscoveryを省いて
  接続する。相手が複数serviceを公開している場合はこちらが必要になる
- 接続成功と接続失敗を別々に受け取る。`connect()`の`true`は試行を開始したという
  意味に過ぎないため
- `0x00`を含むbyte列を送る。SPPは他の値と同様に運ぶ

## 主なAPI

- `bluetooth.spp().connect(address)` — channelを解決して接続する
- `bluetooth.spp().connectToChannel(address, channel)` — channelを指定して接続する
- `onConnectionFailed()` — 受理された後に失敗したことを受け取る
- `write(sessionId, data, length)` / `onData()` — message単位ではなくbyte stream

## Serialコマンド

| キー | 動作 |
|---|---|
| `c` | SDPでchannelを解決して接続 |
| `k` | channel 1へ直接接続 |
| `w` | 0を含む4 byteを送信 |
| `d` | 切断 |

## 関連するガイド

- [Classic入門ガイド §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#4-spp) — RFCOMM channel、複数service、byte stream
- [EspBleを深く使う](../../../docs/GUIDE_ADVANCED.ja.md) — 990 byteのpacket、8件の送信queue、backpressure

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[SppClient.ino](SppClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[SppServer](../SppServer/README.ja.md)です。2台で試す場合は相手に対応するClassicプロファイルを用意し、必要に応じて接続を受ける側を発見可能にして、開始側へそのアドレスを設定します。

## この例の構成

SPPの接続する側。serverは待って見つけられる側だが、clientはaddressを知り、 その先のchannelを解決する必要がある。examples/Classic/SppServerでは示せない 部分がここにある。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `bluetooth.spp().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `bluetooth.spp().onConnectionFailed(...)` — 接続要求が受理されたあとに発生する、相手不在や接続拒否などの失敗を確認します。
4. `bluetooth.spp().onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
5. `bluetooth.spp().onData(...)` — SPP入力はバイトストリームです。1回のコールバックが1メッセージとは限りません。0バイトを含め、長さと独自の区切り規則で処理します。
6. `bluetooth.spp().connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。

## 確認と変更のポイント

serverAddressを置き換えます。通常のconnectはSDPでSPP Channelを調べ、connectToChannelでは呼び出し側がChannelを知っている必要があります。セッション成立後、バイトストリーム上にバイナリのメッセージ区切りを設計します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
