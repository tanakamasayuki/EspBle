// en: A2DP Source: this device sends audio to a speaker or headset. EspBle
//     carries already-encoded SBC frames — it does not encode, so the frames
//     here come from a table. A real sketch takes them from an encoder such as
//     PCMFlowBluetooth. The receiving side is examples/Classic/A2dpSinkRaw.
// ja: A2DP Source。この機器がspeakerやheadsetへ音声を送る側になる。EspBleは
//     encode済みのSBC frameを運ぶだけでencodeはしないため、ここでは固定表の
//     frameを送る。実際のsketchではPCMFlowBluetooth等のencoderから受け取る。
//     受け取る側はexamples/Classic/A2dpSinkRaw。
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
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.a2dpSource().onConnected(
    [](const EspBleClassicA2dpConnection &connection) {
      Serial.printf("Connected to %s, media MTU %u\n",
        connection.peerAddress.c_str(), connection.mediaMtu);
    });
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

  bluetooth.update();
  delay(1);
}
