// DUT for the core-host A2DP interoperability test. EspBle's independently
// built Classic host acts as the A2DP Sink and AVRCP Target; peer_device/ is an
// A2DP Source and AVRCP Controller written against the ESP-IDF Bluedroid API
// Arduino-ESP32 ships. The SBC frames arriving here were negotiated, encoded
// and packetised by that other stack.
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
size_t mediaPackets = 0;
size_t mediaBytes = 0;
uint8_t firstPayloadByte = 0;
unsigned frameCountSum = 0;
bool sinkStarted = false;
bool avrcpStarted = false;
bool connectedState = false;
bool streamingState = false;
uint32_t configuredRate = 0;
uint8_t configuredChannels = 0;
unsigned passthroughKeys = 0;

String classicAddress()
{
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  char text[18];
  snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x", address[0],
    address[1], address[2], address[3], address[4], address[5]);
  return String(text);
}

void reportReady()
{
  Serial.printf("A2DPSINK_READY started=%u avrcp=%u address=%s\n",
    sinkStarted ? 1 : 0, avrcpStarted ? 1 : 0, classicAddress().c_str());
  Serial.printf(
    "A2DPSINK_STATE connected=%u streaming=%u packets=%u bytes=%u rate=%lu "
    "channels=%u keys=%u\n",
    connectedState ? 1 : 0, streamingState ? 1 : 0,
    static_cast<unsigned>(mediaPackets), static_cast<unsigned>(mediaBytes),
    static_cast<unsigned long>(configuredRate), configuredChannels,
    passthroughKeys);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleClassicConfig config;
  config.deviceName = "EspBle CoreHost A2DP Sink";
  config.visibility = EspBleClassicVisibility::ConnectableDiscoverable;
  if (!bluetooth.begin(config))
  {
    Serial.printf("A2DPSINK_INIT_FAILED %s:%s\n", bluetooth.lastErrorName(),
      bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.avrcp().onConnectionChanged(
    [](const EspBleClassicAvrcpConnection &event) {
      Serial.printf("A2DPSINK_AVRCP controller=%u connected=%u\n",
        event.controller ? 1 : 0, event.connected ? 1 : 0);
    });
  bluetooth.avrcp().onPassthrough([](const EspBleClassicAvrcpPassthrough &event) {
    ++passthroughKeys;
    // The Controller on the other stack sent this; command and state must match
    // what it pressed, not a locally invented value.
    Serial.printf("A2DPSINK_KEY command=%u state=%u count=%u\n",
      static_cast<unsigned>(event.command), static_cast<unsigned>(event.state),
      passthroughKeys);
  });
  EspBleClassicAvrcpConfig avrcpConfig;
  avrcpConfig.initialVolume = 64;
  avrcpStarted = bluetooth.avrcp().begin(avrcpConfig);

  bluetooth.a2dpSink().onConnected([](const EspBleClassicA2dpConnection &event) {
    connectedState = true;
    Serial.printf("A2DPSINK_CONNECTED id=%u peer=%s mtu=%u incoming=%u\n",
      event.id, event.peerAddress.c_str(), event.mediaMtu,
      event.incoming ? 1 : 0);
  });
  bluetooth.a2dpSink().onDisconnected([](const EspBleClassicA2dpConnection &event) {
    connectedState = false;
    streamingState = false;
    Serial.printf("A2DPSINK_DISCONNECTED id=%u packets=%u\n", event.id,
      static_cast<unsigned>(mediaPackets));
  });
  bluetooth.a2dpSink().onCodecConfigured(
    [](const EspBleClassicA2dpCodecConfig &event) {
      configuredRate = event.sampleRate;
      configuredChannels = event.channels;
      Serial.printf(
        "A2DPSINK_CODEC codec=%u rate=%lu channels=%u blocks=%u subbands=%u "
        "bitpool=%u-%u\n",
        static_cast<unsigned>(event.codec),
        static_cast<unsigned long>(event.sampleRate), event.channels,
        event.sbcBlockLength, event.sbcSubbands, event.minimumBitpool,
        event.maximumBitpool);
    });
  bluetooth.a2dpSink().onStreamStateChanged(
    [](const EspBleClassicA2dpStreamEvent &event) {
      streamingState = event.state == EspBleClassicA2dpStreamState::Started;
      Serial.printf("A2DPSINK_STREAM state=%u streaming=%u\n",
        static_cast<unsigned>(event.state), streamingState ? 1 : 0);
    });
  bluetooth.a2dpSink().onMedia([](const EspBleClassicEncodedAudioView &view) {
    ++mediaPackets;
    mediaBytes += view.length;
    frameCountSum += view.frameCount;
    if (mediaPackets == 1 && view.length != 0) firstPayloadByte = view.data[0];
  });

  sinkStarted = bluetooth.a2dpSink().begin();
  reportReady();
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "?")
    {
      reportReady();
    }
    else if (command == "q")
    {
      // The SBC syncword 0x9c starts every frame the Source produced, so a
      // Sink that hands over the RTP header or a misaligned payload shows up
      // here rather than as a plausible byte count.
      Serial.printf(
        "A2DPSINK_MEDIA packets=%u bytes=%u frames=%u first=%02x\n",
        static_cast<unsigned>(mediaPackets), static_cast<unsigned>(mediaBytes),
        frameCountSum, firstPayloadByte);
    }
    else if (command == "z")
    {
      mediaPackets = 0;
      mediaBytes = 0;
      frameCountSum = 0;
      firstPayloadByte = 0;
      Serial.println("A2DPSINK_COUNTERS_RESET");
    }
  }

  delay(1);
}
