// en: SPP through an Arduino Stream, for code written against Serial. The
//     session is still opened and closed with the SPP API; the Stream only
//     borrows it, so print(), readStringUntil() and parseInt() work over
//     Bluetooth without giving up the session events.
// ja: SPPをArduinoのStreamとして扱う。Serial向けに書かれたcodeをそのまま使える。
//     sessionを開閉するのは従来どおりSPPのAPIで、Streamはそれを借りるだけなので、
//     print()やreadStringUntil()、parseInt()をBluetooth越しに使いながらsessionの
//     eventも受け取れる。
#include <EspBleClassic.h>

EspBleClassic bluetooth;
EspBleClassicSppStream stream;

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic SPP Stream";
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  // en: A Stream has no address and no connection of its own, so it is attached
  //     when a session opens and detached when it closes. Attaching to a session
  //     that has already gone leaves reads empty and writes at zero rather than
  //     failing loudly.
  // ja: Streamはaddressも接続も持たないため、sessionが開いたら結び付け、閉じたら
  //     外す。既に消えたsessionへ結び付けても、readは空、writeは0になるだけで
  //     派手には失敗しない。
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    stream.attach(bluetooth.spp(), session.id);
    Serial.printf("Connected: %s\n", session.peerAddress.c_str());
    // en: print() sends one packet per call, which is why a line is written in
    //     one call rather than a character at a time.
    // ja: print()は呼び出し1回が1 packetになる。だから1文字ずつではなく行単位で
    //     書く。
    stream.println("EspBle SPP stream ready");
  });
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &session) {
    if (session.id == stream.session()) stream.detach();
    Serial.println("Disconnected");
  });

  EspBleClassicSppServerConfig serverConfig;
  serverConfig.serviceName = "EspBle Stream";
  if (!bluetooth.spp().startServer(serverConfig))
    Serial.printf("startServer failed: %s\n", bluetooth.lastErrorName());

  // en: Stream::setTimeout() governs the read side, as it does for Serial.
  //     setWriteTimeout() is the other half and is specific to this adapter: the
  //     outgoing queue is finite, and this is how long a write waits for room
  //     before reporting what it managed. Zero never waits, which is what a loop
  //     that must not stall wants.
  // ja: 読み側の待ち時間はSerialと同じくStream::setTimeout()で決まる。
  //     setWriteTimeout()は書き側で、このadapter固有である。送信queueは有限なので、
  //     空きを待つ時間を決め、時間切れなら書けた分を返す。0なら待たない——loopを
  //     止めたくない場合はこれを使う。
  stream.setTimeout(200);
  stream.setWriteTimeout(1000);

  Serial.println("Pair, connect a serial terminal, and send lines.");
}

void loop()
{
  bluetooth.update();

  // en: Serial-style reading: a line at a time. available() and read() are the
  //     same session buffer the SPP API exposes, so a sketch can mix the two.
  // ja: Serial流の読み方で、1行ずつ受け取る。available()とread()はSPP APIが見せる
  //     session bufferと同じものなので、両方を混ぜて使ってもよい。
  if (stream.available() > 0)
  {
    const String line = stream.readStringUntil('\n');
    Serial.printf("Peer said: %s\n", line.c_str());
    stream.print("echo: ");
    stream.println(line);
    // en: flush() waits for what was queued to actually reach the peer, bounded
    //     by the write timeout. It is not a no-op here, unlike on some Streams.
    // ja: flush()はqueueに入れた分が実際に相手へ届くまで待つ（write timeoutが上限）。
    //     一部のStreamのような何もしない実装ではない。
    stream.flush();
  }

  delay(1);
}
