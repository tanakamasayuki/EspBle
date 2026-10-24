# SppStream（Classic）

> English: [README.md](README.md)

SPPをArduinoの`Stream`として扱い、`Serial`向けに書かれたcodeをそのまま使えるように
します。sessionを開閉するのは従来どおりSPPのAPIで、Streamはそれを借りるだけなので、
`print()`や`readStringUntil()`、`parseInt()`をBluetooth越しに使いながらsessionのevent
も受け取れます。**Classicは無印ESP32のみ**で動きます。

## 必要なもの

- このsketchを動かす無印ESP32 1台
- SPP client 1台: serial terminalを持つPCやAndroid、または[SppClient](../SppClient/)を
  動かす無印ESP32

## 何をするか

- sessionが開いたらStreamを結び付け、閉じたら外す。Stream自身は接続を持たないため
- `readStringUntil()`で1行ずつ読み、そのまま返す
- `flush()`はqueueに入れたbyteが相手へ届くまで待つ（write timeoutが上限）

## 主なAPI

- `EspBleClassicSppStream stream;` — 未接続のStream
- `stream.attach(bluetooth.spp(), session.id)` / `stream.detach()`
- `stream.setTimeout(ms)` — 読み側。`Serial`と同じ
- `stream.setWriteTimeout(ms)` — 書き側がqueueの空きを待つ時間。0なら待たない
- `stream.connected()` / `stream.session()` — 借りているsessionが開いているか、どれか

## 補足

`Serial`と違う点が2つあります。

- **write 1回が1 packetになります。**`println(line)`は1 packetですが、`write(byte)`を
  ループで呼ぶと1 byteごとに1 packetです。文字単位ではなく行単位で書いてください
- **送信queueは有限です。**空きが無いwriteはwrite timeoutまで待ち、書けた分を返します。
  linkの排出より速く送るsketchでは戻り値を確認してください

`available()`と`read()`はSPP APIが見せるsession bufferと同じものです。
`stream.read()`と`spp().read(sessionId)`を混ぜても構いません——bufferが2つあるのではなく、
1つのbufferに対する2つの見え方です。

## 関連するガイド

- [Classic入門ガイド §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.ja.md#4-spp) — RFCOMM channel、複数service、byte stream
- [EspBleを深く使う](../../../docs/GUIDE_ADVANCED.ja.md) — 990 byteのpacket、8件の送信queue、backpressure

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[SppStream.ino](SppStream.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

SPPをArduinoのStreamとして扱う。Serial向けに書かれたcodeをそのまま使える。 sessionを開閉するのは従来どおりSPPのAPIで、Streamはそれを借りるだけなので、 print()やreadStringUntil()、parseInt()をBluetooth越しに使いながらsessionの eventも受け取れる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `bluetooth.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。Classicプロファイルの構成順は、このスケッチの記述に従います。
2. `bluetooth.spp().onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `bluetooth.spp().onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。

## 確認と変更のポイント

接続中のセッションへStreamをattachし、切断時にdetachします。区切りまでのReadやflush／Write Timeoutはloopを待たせ得るため、updateを回し、組み込み時は行の最大長も決めます。

無印ESP32と、このClassicプロファイルに対応した相手を使います。BLE専用ハードウェアやBLE専用スマートフォンアプリではClassicのSPP・HID・音声の相手を代用できません。基板選択はルートの対応情報を参照します。

`bluetooth.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
