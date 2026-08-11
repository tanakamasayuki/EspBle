#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
size_t mediaPackets = 0;
size_t mediaBytes = 0;
bool teardownRequested = false;

String classicAddress()
{
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  char value[18];
  snprintf(value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  EspBleClassicConfig config;
  config.deviceName = "EspBle Raw A2DP Sink";
  if (!bluetooth.begin(config))
  {
    Serial.printf("A2DP_SINK_STACK_FAILED %s:%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.a2dpSink().onConnected([](const EspBleClassicA2dpConnection &event) {
    Serial.printf("A2DP_SINK_CONNECTED id=%u peer=%s mtu=%u incoming=%u\n",
      event.id, event.peerAddress.c_str(), event.mediaMtu,
      event.incoming ? 1 : 0);
  });
  bluetooth.a2dpSink().onDisconnected([](const EspBleClassicA2dpConnection &event) {
    Serial.printf("A2DP_SINK_DISCONNECTED id=%u packets=%u bytes=%u\n",
      event.id, static_cast<unsigned>(mediaPackets),
      static_cast<unsigned>(mediaBytes));
    bluetooth.a2dpSink().onMedia({});
    Serial.println("A2DP_SINK_MEDIA_UNREGISTERED");
    teardownRequested = true;
  });
  bluetooth.a2dpSink().onCodecConfigured([](const EspBleClassicA2dpCodecConfig &event) {
    Serial.printf(
      "A2DP_SINK_CODEC id=%u codec=%u rate=%lu channels=%u mode=%u blocks=%u "
      "subbands=%u alloc=%u bitpool=%u-%u raw_len=%u\n",
      event.connectionId, static_cast<unsigned>(event.codec),
      static_cast<unsigned long>(event.sampleRate), event.channels,
      static_cast<unsigned>(event.sbcChannelMode), event.sbcBlockLength,
      event.sbcSubbands, static_cast<unsigned>(event.sbcAllocationMethod),
      event.minimumBitpool, event.maximumBitpool,
      static_cast<unsigned>(event.rawLength));
  });
  bluetooth.a2dpSink().onStreamStateChanged([](const EspBleClassicA2dpStreamEvent &event) {
    Serial.printf("A2DP_SINK_STREAM id=%u state=%u\n",
      event.connectionId, static_cast<unsigned>(event.state));
  });
  bluetooth.a2dpSink().onMedia([](const EspBleClassicEncodedAudioView &view) {
    ++mediaPackets;
    mediaBytes += view.length;
    if (mediaPackets <= 3)
    {
      uint32_t checksum = 0;
      for (size_t i = 0; i < view.length; ++i) checksum += view.data[i];
      Serial.printf(
        "A2DP_SINK_MEDIA id=%u codec=%u timestamp=%lu frames=%u len=%u "
        "checksum=%lu first=%02x\n",
        view.connectionId, static_cast<unsigned>(view.codec),
        static_cast<unsigned long>(view.timestamp), view.frameCount,
        static_cast<unsigned>(view.length),
        static_cast<unsigned long>(checksum),
        view.length == 0 ? 0 : view.data[0]);
    }
  });

  const bool started = bluetooth.a2dpSink().begin();
  Serial.printf("A2DP_SINK_READY started=%u address=%s error=%s:%s\n",
    started ? 1 : 0, classicAddress().c_str(), bluetooth.lastErrorName(),
    bluetooth.lastErrorDetail().c_str());
}

void loop()
{
  bluetooth.update();
  if (teardownRequested)
  {
    teardownRequested = false;
    bluetooth.a2dpSink().end();
    Serial.printf("A2DP_SINK_ENDED initialized=%u\n",
      bluetooth.a2dpSink().initialized() ? 1 : 0);
  }
  delay(1);
}
