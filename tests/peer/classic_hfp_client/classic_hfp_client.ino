#include <EspBleClassic.h>
#include <esp_mac.h>
#if defined(ESPBLE_TEST_DUAL_HFP)
#include <EspBle.h>
#include <EspBleHciBroker.h>
#endif

EspBleClassic bluetooth;

#if defined(ESPBLE_TEST_DUAL_HFP)
EspBle dualBle;
EspBleGattService dualService;
EspBleGattCharacteristic dualCharacteristic;
constexpr const char *DualHfpServiceUuid =
  "a6d56000-6807-47b3-8457-bd60344d0001";
constexpr const char *DualHfpCharacteristicUuid =
  "a6d56001-6807-47b3-8457-bd60344d0001";

bool startDualBleServer()
{
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  dualService = dualBle.gattServer().addService(DualHfpServiceUuid);
  dualCharacteristic = dualBle.gattServer().addCharacteristic(
    dualService, DualHfpCharacteristicUuid, characteristicConfig);
  dualBle.gattServer().setValue(dualCharacteristic, String("dual-hfp"));
  dualBle.onConnected([](const EspBleConnection &) {
    Serial.println("DUAL_HFP_BLE_SERVER_CONNECTED");
  });
  dualBle.onDisconnected([](const EspBleConnection &) {
    Serial.println("DUAL_HFP_BLE_SERVER_DISCONNECTED");
  });
  EspBleConfig config;
  config.deviceName = "EspBle Dual HFP";
  if (!dualService.valid() || !dualCharacteristic.valid() ||
      !dualBle.begin(config))
    return false;
  dualBle.advertising().setName("EspBle Dual HFP");
  dualBle.advertising().addServiceUuid(DualHfpServiceUuid);
  return dualBle.advertising().start();
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

  EspBleClassicConfig config;
  config.deviceName = "EspBle HFP Client";
  if (!bluetooth.begin(config))
  {
    Serial.printf("HFP_CLIENT_STACK_FAILED %s\n",
      bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.hfpClient().onConnectionChanged(
    [](const EspBleClassicHfpConnection &event) {
      Serial.printf("HFP_CLIENT_CONNECTION state=%u peer=%s features=%lu\n",
        static_cast<unsigned>(event.state), event.peerAddress.c_str(),
        static_cast<unsigned long>(event.peerFeatures));
    });
  bluetooth.hfpClient().onAudioConnectionChanged(
    [](const EspBleClassicHfpAudioConnection &event) {
      Serial.printf("HFP_CLIENT_AUDIO state=%u codec=%u handle=%u frame=%u\n",
        static_cast<unsigned>(event.state), static_cast<unsigned>(event.codec),
        event.id, event.preferredFrameSize);
    });
  bluetooth.hfpClient().onCallStateChanged(
    [](const EspBleClassicHfpCallState &event) {
      Serial.printf("HFP_CLIENT_CALL active=%u setup=%u held=%u\n",
        event.active ? 1 : 0, static_cast<unsigned>(event.setup),
        static_cast<unsigned>(event.held));
    });
  bluetooth.hfpClient().onAtResponse(
    [](const EspBleClassicHfpAtResponse &event) {
      Serial.printf("HFP_CLIENT_AT code=%u cme=%u\n",
        event.code, event.extendedError);
    });
  bluetooth.hfpClient().onPacketStatistics(
    [](const EspBleClassicHfpPacketStatistics &event) {
      Serial.printf("HFP_CLIENT_STATS rx=%lu ok=%lu bad=%lu tx=%lu drop=%lu\n",
        static_cast<unsigned long>(event.received),
        static_cast<unsigned long>(event.receivedCorrect),
        static_cast<unsigned long>(event.receivedError),
        static_cast<unsigned long>(event.sent),
        static_cast<unsigned long>(event.sentDiscarded));
    });
  bluetooth.hfpClient().onAudio(
    [](const EspBleClassicHfpEncodedAudioView &audio) {
      if (audio.badFrame) return;
      uint32_t checksum = 0;
      for (size_t index = 0; index < audio.length; ++index)
        checksum += audio.data[index];
      Serial.printf("HFP_CLIENT_MEDIA codec=%u handle=%u len=%u bad=%u checksum=%lu\n",
        static_cast<unsigned>(audio.codec), audio.connectionId,
        static_cast<unsigned>(audio.length), audio.badFrame ? 1 : 0,
        static_cast<unsigned long>(checksum));
    });

  if (!bluetooth.hfpClient().begin())
  {
    Serial.printf("HFP_CLIENT_INIT_FAILED %s:%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  const bool gatewayAccepted = bluetooth.hfpAudioGateway().begin();
  Serial.printf("HFP_CLIENT_EXCLUSION ag=%u error=%s\n",
    gatewayAccepted ? 1 : 0, bluetooth.lastErrorName());
#if defined(ESPBLE_TEST_DUAL_HFP)
  if (!startDualBleServer())
  {
    Serial.printf("DUAL_HFP_BLE_START_FAILED %s\n",
      dualBle.lastErrorDetail().c_str());
    return;
  }
  Serial.println("DUAL_HFP_BLE_SERVER_READY");
#endif
  Serial.printf("HFP_CLIENT_READY address=%s\n",
    classicAddress().c_str());
}

void loop()
{
  bluetooth.update();
#if defined(ESPBLE_TEST_DUAL_HFP)
  dualBle.update();
#endif
  if (!Serial.available()) { delay(1); return; }
  const String command = Serial.readStringUntil('\n');
  if (command.startsWith("c"))
    Serial.printf("HFP_CLIENT_CONNECT requested=%u\n",
      bluetooth.hfpClient().connect(command.substring(1).c_str()) ? 1 : 0);
  else if (command == "d")
    Serial.printf("HFP_CLIENT_DIAL requested=%u\n",
      bluetooth.hfpClient().dial("12345") ? 1 : 0);
  else if (command == "a")
    Serial.printf("HFP_CLIENT_AUDIO_CONNECT requested=%u\n",
      bluetooth.hfpClient().connectAudio() ? 1 : 0);
  else if (command == "s")
  {
    uint8_t payload[256];
    const size_t payloadLength =
      bluetooth.hfpClient().audioConnection().preferredFrameSize;
    if (payloadLength == 0 || payloadLength > sizeof(payload))
    {
      Serial.printf("HFP_CLIENT_SEND_INVALID_FRAME frame=%u\n",
        static_cast<unsigned>(payloadLength));
      return;
    }
    for (size_t index = 0; index < payloadLength; ++index)
      payload[index] = static_cast<uint8_t>(index + 1);
    EspBleClassicHfpEncodedAudioPacket packet;
    packet.data = payload;
    packet.length = payloadLength;
    Serial.printf("HFP_CLIENT_SEND result=%u\n",
      static_cast<unsigned>(bluetooth.hfpClient().send(packet)));
  }
  else if (command == "p")
    Serial.printf("HFP_CLIENT_STATS_REQUEST requested=%u\n",
      bluetooth.hfpClient().requestPacketStatistics() ? 1 : 0);
  else if (command == "x")
  {
    Serial.printf("HFP_CLIENT_AUDIO_DISCONNECT requested=%u\n",
      bluetooth.hfpClient().disconnectAudio() ? 1 : 0);
  }
  else if (command == "n")
    Serial.printf("HFP_CLIENT_ANSWER requested=%u\n",
      bluetooth.hfpClient().answerCall() ? 1 : 0);
  else if (command == "h")
    Serial.printf("HFP_CLIENT_HANGUP requested=%u\n",
      bluetooth.hfpClient().rejectOrEndCall() ? 1 : 0);
#if defined(ESPBLE_TEST_DUAL_HFP)
  else if (command == "z")
  {
    espble_hci_broker_diagnostics_t diagnostics = {};
    espble_hci_broker_get_diagnostics(&diagnostics);
    Serial.printf(
      "DUAL_HFP_DIAGNOSTICS acl_tx=%lu,%lu acl_rx=%lu,%lu "
      "unknown=%lu mismatch=%lu qfull=%lu\n",
      static_cast<unsigned long>(diagnostics.tx_acl[0]),
      static_cast<unsigned long>(diagnostics.tx_acl[1]),
      static_cast<unsigned long>(diagnostics.rx_acl[0]),
      static_cast<unsigned long>(diagnostics.rx_acl[1]),
      static_cast<unsigned long>(diagnostics.unknown_acl),
      static_cast<unsigned long>(diagnostics.command_response_mismatch),
      static_cast<unsigned long>(diagnostics.command_queue_full));
  }
#endif
}
