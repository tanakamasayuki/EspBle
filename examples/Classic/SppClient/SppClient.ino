// en: The dialling side of SPP. A server waits and is found; a client has to
//     know an address and then find the channel behind it, which is the part
//     examples/Classic/SppServer cannot show.
// ja: SPPの接続する側。serverは待って見つけられる側だが、clientはaddressを知り、
//     その先のchannelを解決する必要がある。examples/Classic/SppServerでは示せない
//     部分がここにある。
#include <EspBleClassic.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sessionId = 0;

// en: Replace with the server's address, or find one with
//     examples/Classic/Inquiry.
// ja: serverのaddressに置き換える。examples/Classic/Inquiryで探すこともできる。
const char *serverAddress = "00:00:00:00:00:00";

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic SPP Client";
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sessionId = session.id;
    Serial.printf("Connected to %s (session %u)\n",
      session.peerAddress.c_str(), static_cast<unsigned>(session.id));
    bluetooth.spp().write(session.id, "hello");
  });
  // en: connect() returning true only means the attempt started. It can still
  //     fail afterwards — no such device, no SPP service, pairing refused — and
  //     that arrives here.
  // ja: connect()の`true`は試行を開始したという意味に過ぎない。相手が居ない、
  //     SPPを公開していない、pairingを断られた等で後から失敗し、それはここへ届く。
  bluetooth.spp().onConnectionFailed(
    [](const EspBleClassicSppConnectionFailure &failure) {
      Serial.printf("Connection to %s failed: %s\n",
        failure.peerAddress.c_str(), failure.detail.c_str());
    });
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &session) {
    if (session.id == sessionId) sessionId = 0;
    Serial.println("Disconnected");
  });
  // en: SPP is a byte stream, not a message channel: what arrives here is
  //     whatever has been received so far, and a zero byte is data like any
  //     other.
  // ja: SPPはbyte streamでmessage単位ではない。ここへ届くのはその時点までに
  //     受信した分で、`0x00`も他と同じ1 byteのデータである。
  bluetooth.spp().onData([](const EspBleClassicSppData &event) {
    Serial.printf("Received %u bytes\n",
      static_cast<unsigned>(event.value.length()));
  });

  Serial.println("Send 'c' to connect, 'k' to connect to channel 1, 'd', 'w'.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'c')
    {
      // en: Without a channel, the library asks the peer over SDP which channel
      //     its SPP service is on and connects to the first one it publishes.
      // ja: channelを指定しない場合、libraryがSDPで相手のSPP serviceのchannelを
      //     問い合わせ、最初に公開されているものへ接続する。
      Serial.printf("connect requested=%u error=%s\n",
        bluetooth.spp().connect(serverAddress) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 'k')
    {
      // en: A peer that publishes several services offers several channels, and
      //     discovery cannot say which one is wanted. Naming the channel skips
      //     discovery, so the caller has to know it.
      // ja: 複数serviceを公開する相手はchannelも複数返し、discoveryだけでは
      //     どれを使うか決められない。channel指定はdiscoveryを省くため、
      //     呼び出し側が値を知っている必要がある。
      Serial.printf("connect requested=%u error=%s\n",
        bluetooth.spp().connectToChannel(serverAddress, 1) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 'w' && sessionId != 0)
    {
      const uint8_t payload[] = {0x00, 0x01, 0x02, 0xff};
      bluetooth.spp().write(sessionId, payload, sizeof(payload));
    }
    else if (command == 'd' && sessionId != 0)
    {
      bluetooth.spp().disconnect(sessionId);
    }
  }

  bluetooth.update();
  delay(1);
}
