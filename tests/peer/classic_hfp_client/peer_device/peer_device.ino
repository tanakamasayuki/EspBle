#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
bool audioEchoed = false;

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

  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "EspBle HFP AG Probe";
  if (!bluetooth.begin(classicConfig))
  {
    Serial.printf("HFP_AG_STACK_FAILED %s\n",
      bluetooth.lastErrorDetail().c_str());
    return;
  }

  auto &gateway = bluetooth.hfpAudioGateway();
  gateway.onConnectionChanged([](const EspBleClassicHfpConnection &event) {
    Serial.printf("HFP_AG_CONNECTION state=%u peer=%s features=%lu\n",
      static_cast<unsigned>(event.state), event.peerAddress.c_str(),
      static_cast<unsigned long>(event.peerFeatures));
  });
  gateway.onAudioConnectionChanged(
    [](const EspBleClassicHfpAudioConnection &event) {
      Serial.printf("HFP_AG_AUDIO state=%u codec=%u handle=%u frame=%u\n",
        static_cast<unsigned>(event.state), static_cast<unsigned>(event.codec),
        event.id, event.preferredFrameSize);
    });
  gateway.onCallStateChanged([](const EspBleClassicHfpCallState &event) {
    Serial.printf("HFP_AG_CALL active=%u setup=%u held=%u\n",
      event.active ? 1 : 0, static_cast<unsigned>(event.setup),
      static_cast<unsigned>(event.held));
  });
  gateway.onCommand([&gateway](
    const EspBleClassicHfpAudioGatewayCommand &command) {
    if (command.type == EspBleClassicHfpAudioGatewayCommandType::Dial)
    {
      Serial.printf("HFP_AG_DIAL number=%s\n", command.value.c_str());
      const bool accepted = gateway.respondToCommand(true);
      const bool dialing = gateway.reportOutgoingCall(command.value.c_str());
      const bool active = dialing && gateway.reportCallActive();
      Serial.printf("HFP_AG_DIAL_RESPONSE accepted=%u active=%u\n",
        accepted ? 1 : 0, active ? 1 : 0);
    }
    else if (command.type == EspBleClassicHfpAudioGatewayCommandType::Answer)
    {
      const bool active = gateway.reportCallActive();
      Serial.printf("HFP_AG_ANSWER active=%u\n", active ? 1 : 0);
    }
    else if (command.type == EspBleClassicHfpAudioGatewayCommandType::Hangup)
    {
      const bool ended = gateway.reportCallEnded();
      Serial.printf("HFP_AG_HANGUP ended=%u\n", ended ? 1 : 0);
    }
  });
  gateway.onPacketStatistics(
    [](const EspBleClassicHfpPacketStatistics &statistics) {
      Serial.printf("HFP_AG_STATS rx=%lu ok=%lu bad=%lu tx=%lu drop=%lu\n",
        static_cast<unsigned long>(statistics.received),
        static_cast<unsigned long>(statistics.receivedCorrect),
        static_cast<unsigned long>(statistics.receivedError),
        static_cast<unsigned long>(statistics.sent),
        static_cast<unsigned long>(statistics.sentDiscarded));
    });
  gateway.onAudio([&gateway](const EspBleClassicHfpEncodedAudioView &view) {
    if (view.badFrame) return;
    uint32_t checksum = 0;
    for (size_t index = 0; index < view.length; ++index)
      checksum += view.data[index];
    Serial.printf("HFP_AG_MEDIA handle=%u len=%u bad=%u checksum=%lu\n",
      view.connectionId, view.length, view.badFrame ? 1 : 0,
      static_cast<unsigned long>(checksum));
    if (audioEchoed) return;
    audioEchoed = true;
    EspBleClassicHfpEncodedAudioPacket packet;
    packet.data = view.data;
    packet.length = min(view.length, static_cast<size_t>(57));
    const auto result = gateway.send(packet);
    Serial.printf("HFP_AG_SEND result=%u\n", static_cast<unsigned>(result));
  });

  EspBleClassicHfpAudioGatewayConfig gatewayConfig;
  gatewayConfig.operatorName = "EspBle";
  gatewayConfig.subscriberNumber = "5550000";
  if (!gateway.begin(gatewayConfig))
  {
    Serial.printf("HFP_AG_INIT_FAILED %s\n",
      bluetooth.lastErrorDetail().c_str());
    return;
  }
  const bool clientAccepted = bluetooth.hfpClient().begin();
  Serial.printf("HFP_AG_EXCLUSION client=%u error=%s\n",
    clientAccepted ? 1 : 0, bluetooth.lastErrorName());
  Serial.printf("HFP_AG_READY address=%s\n", classicAddress().c_str());
}

void loop()
{
  bluetooth.update();
  if (Serial.available())
  {
    const String command = Serial.readStringUntil('\n');
    if (command == "i")
      Serial.printf("HFP_AG_INCOMING reported=%u\n",
        bluetooth.hfpAudioGateway().reportIncomingCall("54321") ? 1 : 0);
  }
  delay(1);
}
