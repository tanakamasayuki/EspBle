#include <EspBleClassic.h>
#include <esp_mac.h>
#if defined(ESPBLE_TEST_DUAL_A2DP)
#include <EspBle.h>
#include <EspBleHciBroker.h>
#endif

EspBleClassic bluetooth;
size_t mediaPackets = 0;
size_t mediaBytes = 0;
bool teardownRequested = false;

#if defined(ESPBLE_TEST_DUAL_A2DP)
EspBle dualBle;
EspBleGattService dualService;
EspBleGattCharacteristic dualCharacteristic;
constexpr const char *DualA2dpServiceUuid =
  "6fd8e000-6548-49b7-a32d-b0240fa70001";
constexpr const char *DualA2dpCharacteristicUuid =
  "6fd8e001-6548-49b7-a32d-b0240fa70001";

bool startDualBleServer()
{
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  dualService = dualBle.gattServer().addService(DualA2dpServiceUuid);
  dualCharacteristic = dualBle.gattServer().addCharacteristic(
    dualService, DualA2dpCharacteristicUuid, characteristicConfig);
  dualBle.gattServer().setValue(dualCharacteristic, String("dual-a2dp"));
  dualBle.onConnected([](const EspBleConnection &) {
    Serial.println("DUAL_A2DP_BLE_SERVER_CONNECTED");
  });
  dualBle.onDisconnected([](const EspBleConnection &) {
    Serial.println("DUAL_A2DP_BLE_SERVER_DISCONNECTED");
  });
  EspBleConfig config;
  config.deviceName = "EspBle Dual A2DP";
  if (!dualService.valid() || !dualCharacteristic.valid() ||
      !dualBle.begin(config))
    return false;
  dualBle.advertising().setName("EspBle Dual A2DP");
  dualBle.advertising().addServiceUuid(DualA2dpServiceUuid);
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
  config.deviceName = "EspBle Raw A2DP Sink";
  if (!bluetooth.begin(config))
  {
    Serial.printf("A2DP_SINK_STACK_FAILED %s:%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.avrcp().onConnectionChanged(
    [](const EspBleClassicAvrcpConnection &event) {
      Serial.printf("AVRCP_SINK_CONNECTION controller=%u connected=%u peer=%s\n",
        event.controller ? 1 : 0, event.connected ? 1 : 0,
        event.peerAddress.c_str());
    });
  bluetooth.avrcp().onPassthrough(
    [](const EspBleClassicAvrcpPassthrough &event) {
      Serial.printf("AVRCP_SINK_KEY command=%u state=%u\n",
        static_cast<unsigned>(event.command),
        static_cast<unsigned>(event.state));
    });
  bluetooth.avrcp().onVolumeChanged(
    [](const EspBleClassicAvrcpVolume &event) {
      Serial.printf("AVRCP_SINK_VOLUME value=%u remote=%u\n",
        event.value, event.remoteCommand ? 1 : 0);
      if (event.remoteCommand && event.value == 77)
        Serial.printf("AVRCP_SINK_LOCAL_VOLUME changed=%u\n",
          bluetooth.avrcp().setLocalVolume(88) ? 1 : 0);
    });
  EspBleClassicAvrcpConfig avrcpConfig;
  avrcpConfig.initialVolume = 64;
  if (!bluetooth.avrcp().begin(avrcpConfig))
  {
    Serial.printf("AVRCP_SINK_INIT_FAILED %s:%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  Serial.println("AVRCP_SINK_READY");

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
#if defined(ESPBLE_TEST_DUAL_A2DP)
  if (!started || !startDualBleServer())
  {
    Serial.printf("DUAL_A2DP_BLE_START_FAILED %s\n",
      dualBle.lastErrorDetail().c_str());
    return;
  }
  Serial.println("DUAL_A2DP_BLE_SERVER_READY");
#endif
}

void loop()
{
  bluetooth.update();
#if defined(ESPBLE_TEST_DUAL_A2DP)
  dualBle.update();
#endif
  if (teardownRequested)
  {
    teardownRequested = false;
    bluetooth.avrcp().end();
    bluetooth.a2dpSink().end();
    Serial.printf("A2DP_SINK_ENDED initialized=%u\n",
      bluetooth.a2dpSink().initialized() ? 1 : 0);
  }
#if defined(ESPBLE_TEST_DUAL_A2DP)
  if (Serial.available())
  {
    const String command = Serial.readStringUntil('\n');
    if (command == "z")
    {
      espble_hci_broker_diagnostics_t diagnostics = {};
      espble_hci_broker_get_diagnostics(&diagnostics);
      bool coexCommandSeen = false;
      for (size_t i = 0; i < diagnostics.command_opcode_count[1]; ++i)
      {
        if (diagnostics.command_opcodes[1][i] == 0xfc82)
        {
          coexCommandSeen = true;
          break;
        }
      }
      Serial.printf(
        "DUAL_A2DP_DIAGNOSTICS acl_tx=%lu,%lu acl_rx=%lu,%lu "
        "unknown=%lu mismatch=%lu qfull=%lu coex=%u\n",
        static_cast<unsigned long>(diagnostics.tx_acl[0]),
        static_cast<unsigned long>(diagnostics.tx_acl[1]),
        static_cast<unsigned long>(diagnostics.rx_acl[0]),
        static_cast<unsigned long>(diagnostics.rx_acl[1]),
        static_cast<unsigned long>(diagnostics.unknown_acl),
        static_cast<unsigned long>(diagnostics.command_response_mismatch),
        static_cast<unsigned long>(diagnostics.command_queue_full),
        coexCommandSeen ? 1 : 0);
    }
  }
#endif
  delay(1);
}
