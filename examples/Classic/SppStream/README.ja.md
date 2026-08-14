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
