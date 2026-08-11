#include <EspBleClassic.h>
#include <esp_mac.h>
#if defined(ESPBLE_TEST_DUAL_HFP)
#include <EspBle.h>
#endif

EspBleClassic bluetooth;
bool audioEchoed = false;

#if defined(ESPBLE_TEST_DUAL_HFP)
EspBle dualBle;
EspBleConnectionId dualConnectionId = 0;
bool dualConnectionRequested = false;
constexpr const char *DualHfpServiceUuid =
  "a6d56000-6807-47b3-8457-bd60344d0001";
constexpr const char *DualHfpCharacteristicUuid =
  "a6d56001-6807-47b3-8457-bd60344d0001";

bool startDualBleClient()
{
  dualBle.scanner().onResult([](const EspBleScanResult &result) {
    if (dualConnectionRequested ||
        !result.advertisesService(DualHfpServiceUuid)) return;
    dualBle.scanner().stop();
    dualConnectionRequested = dualBle.connect(result);
    Serial.printf("DUAL_HFP_BLE_CONNECT requested=%u\n",
      dualConnectionRequested ? 1 : 0);
  });
  dualBle.onConnected([](const EspBleConnection &connection) {
    dualConnectionId = connection.id;
    Serial.println("DUAL_HFP_BLE_CLIENT_CONNECTED");
    Serial.printf("DUAL_HFP_BLE_READ_REQUESTED %u\n",
      dualBle.readCharacteristic(
        connection.id, DualHfpServiceUuid, DualHfpCharacteristicUuid) ? 1 : 0);
  });
  dualBle.onDisconnected([](const EspBleConnection &) {
    dualConnectionId = 0;
    dualConnectionRequested = false;
    Serial.println("DUAL_HFP_BLE_CLIENT_DISCONNECTED");
  });
  dualBle.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf("DUAL_HFP_BLE_READ success=%u value=%s hfp=%u\n",
      result.success ? 1 : 0, result.value.c_str(),
      bluetooth.hfpAudioGateway().audioConnected() ? 1 : 0);
  });
  EspBleConfig config;
  config.deviceName = "EspBle Dual HFP Peer";
  if (!dualBle.begin(config)) return false;
  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  return dualBle.scanner().start(scanConfig);
}
#endif

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
#if defined(ESPBLE_TEST_DUAL_HFP)
  if (!startDualBleClient())
  {
    Serial.printf("DUAL_HFP_BLE_START_FAILED %s\n",
      dualBle.lastErrorDetail().c_str());
    return;
  }
  Serial.println("DUAL_HFP_BLE_CLIENT_READY");
#endif
  Serial.printf("HFP_AG_READY address=%s\n", classicAddress().c_str());
}

void loop()
{
  bluetooth.update();
#if defined(ESPBLE_TEST_DUAL_HFP)
  dualBle.update();
#endif
  if (Serial.available())
  {
    const String command = Serial.readStringUntil('\n');
    if (command == "i")
      Serial.printf("HFP_AG_INCOMING reported=%u\n",
        bluetooth.hfpAudioGateway().reportIncomingCall("54321") ? 1 : 0);
#if defined(ESPBLE_TEST_DUAL_HFP)
    else if (command == "r")
      Serial.printf("DUAL_HFP_BLE_READ_REQUESTED %u\n",
        dualConnectionId != 0 && dualBle.readCharacteristic(
          dualConnectionId, DualHfpServiceUuid, DualHfpCharacteristicUuid)
          ? 1 : 0);
#endif
  }
  delay(1);
}
