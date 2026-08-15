#include <EspBleClassic.h>

EspBleClassic bluetooth;
volatile uint32_t packetCount = 0;
volatile uint32_t byteCount = 0;

void setup()
{
  Serial.begin(115200);

  bluetooth.a2dpSink().onConnected(
    [](const EspBleClassicA2dpConnection &connection) {
      Serial.printf(
        "A2DP connected: %s, MTU %u\n",
        connection.peerAddress.c_str(), connection.mediaMtu);
    });
  bluetooth.a2dpSink().onCodecConfigured(
    [](const EspBleClassicA2dpCodecConfig &codec) {
      Serial.printf(
        "SBC configuration: %lu Hz, %u channel(s), bitpool %u-%u\n",
        static_cast<unsigned long>(codec.sampleRate), codec.channels,
        codec.minimumBitpool, codec.maximumBitpool);
    });
  bluetooth.a2dpSink().onMedia(
    [](const EspBleClassicEncodedAudioView &audio) {
      // audio.data is valid only in this callback. Copy it into a bounded
      // queue here when a decoder such as PCMFlowBluetooth consumes it.
      ++packetCount;
      byteCount += audio.length;
    });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle A2DP Sink";
  if (!bluetooth.begin(stackConfig) || !bluetooth.a2dpSink().begin())
    Serial.println(bluetooth.lastErrorDetail());
}

void loop()
{
  bluetooth.update();
  static uint32_t nextReport = 0;
  if (static_cast<int32_t>(millis() - nextReport) >= 0)
  {
    Serial.printf("encoded packets=%lu bytes=%lu\n",
      static_cast<unsigned long>(packetCount),
      static_cast<unsigned long>(byteCount));
    nextReport = millis() + 1000;
  }
  delay(1);
}
