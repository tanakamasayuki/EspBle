# SppServer（Classic）

> English: [README.md](README.md)

SPPの待ち受け側です。Classic上のbinary-safeなbyte streamで、PCやAndroidからは
serial portとして見えます。**Classicは無印ESP32のみ**で動き、届く相手はClassic機器
——PC、Android、他のESP32です。**iOSのアプリからSPPは使えません**（MFi機器を除く）。
iPhone相手なら独自GATT serviceのBLEにします。`Serial`向けに書かれたcodeをそのまま
使いたい場合は[SppStream](../SppStream/)を参照してください。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- SPP client 1台: serial terminalを持つPCやAndroid、または[SppClient](../SppClient/)を
  動かす無印ESP32

## 何をするか

- SPP serviceを1つ公開して待つ
- 受け取ったbyteをそのまま返す
- 開いているsessionを覚え、書き込み先を把握する

## 主なAPI

- `bluetooth.spp().startServer()` — serviceを公開する。繰り返し呼べば最大4つ公開でき、
  それぞれ別のRFCOMM channelを得る
- `onConnected()` / `onDisconnected()` — session id。読み書きはすべてこのidを指定する
- `onData()` — 受信したbyte。`available()` / `read()`は同じbufferをstreamとして読む形
- `write(sessionId, value)` — queueへ入るだけで、送信完了は`onWriteCompleted()`

## 補足

SPPはbinary-safeです。payload途中の`0x00`は終端ではなくデータです。

SPP自体はpairingを必須としませんが、相手が要求することがあります。sketchから制御する
場合は[SppPairing](../SppPairing/)を参照してください。

## 関連するガイド

- [Classic入門ガイド §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#4-spp) — RFCOMM channel、複数service、byte stream
- [EspBleを深く使う](../../../docs/GUIDE_ADVANCED.ja.md) — 990 byteのpacket、8件の送信queue、backpressure

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[SppServer.ino](SppServer.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[SppClient](../SppClient/README.ja.md)です。2台で試す場合は相手に対応するClassicプロファイルを用意し、必要に応じて接続を受ける側を発見可能にして、開始側へそのアドレスを設定します。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `bluetooth.spp().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `bluetooth.spp().onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
4. `bluetooth.spp().onData(...)` — SPP入力はバイトストリームです。1回のコールバックが1メッセージとは限りません。0バイトを含め、長さと独自の区切り規則で処理します。

## 確認と変更のポイント

名前付きSPP Serverを開始し、Classic SPP Clientから接続します。受信チャンクはアプリのメッセージを分割・結合し得るため、エコーを拡張する際は容量制限付きの解析処理を用意します。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
