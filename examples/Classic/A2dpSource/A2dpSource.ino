// en: Send encoded SBC frames with backpressure
// ja: encode済みSBC frameをbackpressure付きで送信
#include <EspBleClassic.h>

EspBleClassic bluetooth;
bool streaming = false;

// en: Replace with the speaker's address, or find one with
//     examples/Classic/Inquiry.
// ja: speakerのaddressに置き換える。examples/Classic/Inquiryで探すこともできる。
const char *speakerAddress = "00:00:00:00:00:00";

// en: One SBC frame. Real frames come from an encoder configured to match the
//     negotiated stream; this fixed one only shows the transport.
// ja: SBC frame 1つ。実際のframeはnegotiationに合わせて設定したencoderから来る。
//     ここでは転送経路を示すための固定値。
static const uint8_t SbcFrame[] = {
  0x9c, 0x3b, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00};

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic A2DP Source";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  bluetooth.a2dpSource().onConnected(
    [](const EspBleClassicA2dpConnection &connection) {
      Serial.printf("Connected to %s, media MTU %u\n",
        connection.peerAddress.c_str(), connection.mediaMtu);
    });
  // en: Use the negotiated codec configuration; raw encoded packets are not PCM samples and need a matching external codec pipeline.
  // ja: 交渉で決まったコーデック設定を使います。符号化パケットはPCMサンプルではないので、対応する外部コーデック処理が必要です。
  bluetooth.a2dpSource().onCodecConfigured(
    [](const EspBleClassicA2dpCodecConfig &codec) {
      // en: The Sink chooses from what this Source offered, so the encoder has
      //     to follow this, not the other way round.
      // ja: SinkはこのSourceが提示した候補から選ぶため、encoderはこの結果に
      //     合わせる。逆ではない。
      Serial.printf("Negotiated %lu Hz, %u channels, bitpool %u-%u\n",
        static_cast<unsigned long>(codec.sampleRate), codec.channels,
        codec.minimumBitpool, codec.maximumBitpool);
    });
  bluetooth.a2dpSource().onStreamStateChanged(
    [](const EspBleClassicA2dpStreamEvent &event) {
      streaming = event.state == EspBleClassicA2dpStreamState::Started;
      Serial.printf("Stream %s\n", streaming ? "started" : "suspended");
    });
  // en: How long the Sink takes to play what it receives. A sketch showing
  //     video delays the picture by this much.
  // ja: Sinkが受け取った音を再生するまでの時間。映像を出すsketchはこの分だけ
  //     絵を遅らせる。
  bluetooth.a2dpSource().onSinkDelay([](const EspBleClassicA2dpDelay &delay) {
    Serial.printf("Sink reports %u tenths of a millisecond\n",
      delay.tenthsOfMilliseconds);
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  bluetooth.a2dpSource().onDisconnected(
    [](const EspBleClassicA2dpConnection &) {
      streaming = false;
      Serial.println("Disconnected");
    });

  if (!bluetooth.a2dpSource().begin())
  {
    Serial.printf("A2DP Source failed: %s\n",
      bluetooth.lastErrorDetail().c_str());
    return;
  }

  Serial.println("Send 'c' to connect, 's' to start, 'p' to suspend.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'c') bluetooth.a2dpSource().connect(speakerAddress);
    else if (command == 's') bluetooth.a2dpSource().start();
    else if (command == 'p') bluetooth.a2dpSource().suspend();
    else if (command == 'd') bluetooth.a2dpSource().disconnect();
  }

  if (streaming)
  {
    EspBleClassicEncodedAudioPacket packet;
    packet.data = SbcFrame;
    packet.length = sizeof(SbcFrame);
    packet.frameCount = 1;
    const EspBleClassicAudioSendResult result =
      bluetooth.a2dpSource().send(packet);
    // en: WouldBlock is normal backpressure, not an error: the radio has not
    //     drained what was already queued. The frame has to be kept and retried
    //     rather than dropped, or the stream develops gaps.
    // ja: WouldBlockは正常なbackpressureでerrorではない。queue済みの分が
    //     まだ送り切れていないという意味である。frameは捨てずに保持して再送する。
    //     捨てるとstreamに欠落が出る。
    if (result != EspBleClassicAudioSendResult::Accepted &&
        result != EspBleClassicAudioSendResult::WouldBlock)
    {
      Serial.printf("send failed: %u\n", static_cast<unsigned>(result));
    }
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  delay(1);
}
