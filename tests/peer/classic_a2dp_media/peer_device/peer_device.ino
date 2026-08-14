#include <EspBleClassic.h>
#include <esp_heap_caps.h>
#if defined(ESPBLE_TEST_DUAL_A2DP)
#include <EspBle.h>
#endif

EspBleClassic bluetooth;
uint32_t nextTimestamp = 1000;
size_t sentPackets = 0;
size_t wouldBlockCount = 0;
uint32_t drainDeadline = 0;
bool suspendRequested = false;
bool a2dpConnected = false;
bool avrcpControllerConnected = false;
bool avrcpCommandsSent = false;
bool teardownRequested = false;
size_t packetTarget = 100;
uint32_t baselineHeap = 0;
constexpr size_t MaximumPacketTarget = 500000;

#if defined(ESPBLE_TEST_DUAL_A2DP)
EspBle dualBle;
EspBleConnectionId dualConnectionId = 0;
bool dualConnectionRequested = false;
constexpr const char *DualA2dpServiceUuid =
  "6fd8e000-6548-49b7-a32d-b0240fa70001";
constexpr const char *DualA2dpCharacteristicUuid =
  "6fd8e001-6548-49b7-a32d-b0240fa70001";

bool startDualBleClient()
{
  dualBle.scanner().onResult([](const EspBleScanResult &result) {
    if (dualConnectionRequested ||
        !result.advertisesService(DualA2dpServiceUuid)) return;
    dualBle.scanner().stop();
    dualConnectionRequested = dualBle.connect(result);
    Serial.printf("DUAL_A2DP_BLE_CONNECT requested=%u\n",
      dualConnectionRequested ? 1 : 0);
  });
  dualBle.onConnected([](const EspBleConnection &connection) {
    dualConnectionId = connection.id;
    Serial.println("DUAL_A2DP_BLE_CLIENT_CONNECTED");
    Serial.printf("DUAL_A2DP_BLE_READ_REQUESTED %u\n",
      dualBle.readCharacteristic(
        connection.id, DualA2dpServiceUuid,
        DualA2dpCharacteristicUuid) ? 1 : 0);
  });
  dualBle.onDisconnected([](const EspBleConnection &) {
    dualConnectionId = 0;
    dualConnectionRequested = false;
    Serial.println("DUAL_A2DP_BLE_CLIENT_DISCONNECTED");
  });
  dualBle.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf("DUAL_A2DP_BLE_READ success=%u value=%s a2dp=%u\n",
      result.success ? 1 : 0, result.value.c_str(),
      bluetooth.a2dpSource().streaming() ? 1 : 0);
  });
  EspBleConfig config;
  config.deviceName = "EspBle Dual A2DP Peer";
  if (!dualBle.begin(config)) return false;
  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  return dualBle.scanner().start(scanConfig);
}
#endif

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.a2dpSource().onConnected(
    [](const EspBleClassicA2dpConnection &connection) {
      Serial.printf("A2DP_SOURCE_CONNECTED id=%u mtu=%u\n",
        connection.id, connection.mediaMtu);
      a2dpConnected = true;
    });
  bluetooth.a2dpSource().onSinkDelay([](const EspBleClassicA2dpDelay &delay) {
    // The Sink reports this on its own; a video player would use it to hold
    // pictures back by the same amount.
    Serial.printf("A2DP_SOURCE_SINK_DELAY value=%u\n",
      delay.tenthsOfMilliseconds);
  });
  bluetooth.a2dpSource().onCodecConfigured(
    [](const EspBleClassicA2dpCodecConfig &codec) {
      Serial.printf("A2DP_SOURCE_CODEC rate=%lu channels=%u\n",
        static_cast<unsigned long>(codec.sampleRate), codec.channels);
    });
  bluetooth.a2dpSource().onStreamStateChanged(
    [](const EspBleClassicA2dpStreamEvent &stream) {
      Serial.printf("A2DP_SOURCE_STREAM state=%u\n",
        static_cast<unsigned>(stream.state));
      if (stream.state == EspBleClassicA2dpStreamState::Suspended &&
          suspendRequested)
        bluetooth.a2dpSource().disconnect();
    });
  bluetooth.a2dpSource().onDisconnected(
    [](const EspBleClassicA2dpConnection &) {
      a2dpConnected = false;
      Serial.printf("A2DP_SOURCE_DISCONNECTED sent=%u would_block=%u\n",
        static_cast<unsigned>(sentPackets),
        static_cast<unsigned>(wouldBlockCount));
      teardownRequested = true;
    });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle Raw A2DP Source Peer";
  if (!bluetooth.begin(stackConfig))
  {
    Serial.printf("A2DP_SOURCE_STACK_FAILED %s:%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  bluetooth.avrcp().onConnectionChanged(
    [](const EspBleClassicAvrcpConnection &event) {
      Serial.printf("AVRCP_SOURCE_CONNECTION controller=%u connected=%u peer=%s\n",
        event.controller ? 1 : 0, event.connected ? 1 : 0,
        event.peerAddress.c_str());
      if (event.controller) avrcpControllerConnected = event.connected;
    });
  bluetooth.avrcp().onPassthroughResponse(
    [](const EspBleClassicAvrcpPassthroughResponse &event) {
      Serial.printf("AVRCP_SOURCE_KEY_RESPONSE command=%u state=%u accepted=%u\n",
        static_cast<unsigned>(event.command),
        static_cast<unsigned>(event.state), event.accepted ? 1 : 0);
    });
  bluetooth.avrcp().onVolumeChanged(
    [](const EspBleClassicAvrcpVolume &event) {
      Serial.printf("AVRCP_SOURCE_VOLUME value=%u remote=%u\n",
        event.value, event.remoteCommand ? 1 : 0);
    });
  bluetooth.avrcp().onPlayStatus(
    [](const EspBleClassicAvrcpPlayStatus &status) {
      Serial.printf("AVRCP_SOURCE_PLAY_STATUS state=%u position=%lu\n",
        static_cast<unsigned>(status.state),
        static_cast<unsigned long>(status.positionMilliseconds));
    });
  if (!bluetooth.avrcp().begin())
  {
    Serial.printf("AVRCP_SOURCE_INIT_FAILED %s:%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  Serial.println("AVRCP_SOURCE_READY");
  const bool initialized = bluetooth.a2dpSource().begin();
  baselineHeap = ESP.getFreeHeap();
  Serial.printf("A2DP_SOURCE_PROFILE initialized=%u\n",
    initialized ? 1 : 0);
  Serial.printf("A2DP_SOURCE_READY endpoint=%u seid=0\n",
    initialized ? 1 : 0);
#if defined(ESPBLE_TEST_DUAL_A2DP)
  if (!initialized || !startDualBleClient())
  {
    Serial.printf("DUAL_A2DP_BLE_START_FAILED %s\n",
      dualBle.lastErrorDetail().c_str());
    return;
  }
  Serial.println("DUAL_A2DP_BLE_CLIENT_READY");
#endif
}

EspBleClassicAudioSendResult sendPacket()
{
  static const uint8_t Payload[] = {
    0x9c, 0xbd, 0x20, 0x35, 0x00, 0x11, 0x22,
    0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
  };
  EspBleClassicEncodedAudioPacket packet;
  packet.data = Payload;
  packet.length = sizeof(Payload);
  packet.frameCount = 1;
  packet.timestamp = nextTimestamp;
  const EspBleClassicAudioSendResult result =
    bluetooth.a2dpSource().send(packet);
  if (result == EspBleClassicAudioSendResult::Accepted)
  {
    ++sentPackets;
    nextTimestamp += 128;
    if (sentPackets <= 3 || sentPackets == packetTarget)
      Serial.printf("A2DP_SOURCE_SENT packet=%u timestamp=%lu\n",
        static_cast<unsigned>(sentPackets),
        static_cast<unsigned long>(packet.timestamp));
  }
  else if (result == EspBleClassicAudioSendResult::WouldBlock)
    ++wouldBlockCount;
  else
  {
    Serial.printf("A2DP_SOURCE_SEND_FAILED result=%u\n",
      static_cast<unsigned>(result));
  }
  return result;
}

void loop()
{
  bluetooth.update();
#if defined(ESPBLE_TEST_DUAL_A2DP)
  dualBle.update();
#endif
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.startsWith("c"))
      Serial.printf("A2DP_SOURCE_CONNECT requested=%u\n",
        bluetooth.a2dpSource().connect(command.c_str() + 1) ? 1 : 0);
    else if (command.startsWith("v") && a2dpConnected &&
             avrcpControllerConnected && !avrcpCommandsSent)
    {
      if (command.length() > 1)
      {
        const unsigned long requested = command.substring(1).toInt();
        if (requested == 0 || requested > MaximumPacketTarget)
        {
          Serial.printf("A2DP_SOURCE_TARGET_REJECTED value=%lu\n", requested);
          delay(1);
          return;
        }
        packetTarget = static_cast<size_t>(requested);
      }
      avrcpCommandsSent = true;
      Serial.printf("A2DP_SOURCE_TARGET packets=%u\n",
        static_cast<unsigned>(packetTarget));
      Serial.printf("AVRCP_SOURCE_REGISTER_VOLUME requested=%u\n",
        bluetooth.avrcp().registerVolumeNotifications() ? 1 : 0);
      Serial.printf("AVRCP_SOURCE_PLAY requested=%u\n",
        bluetooth.avrcp().sendKey(EspBleClassicAvrcpCommand::Play) ? 1 : 0);
      Serial.printf("AVRCP_SOURCE_SET_VOLUME requested=%u\n",
        bluetooth.avrcp().setAbsoluteVolume(77) ? 1 : 0);
      Serial.printf("A2DP_SOURCE_START requested=%u\n",
        bluetooth.a2dpSource().start() ? 1 : 0);
    }
    else if (command == "n")
      Serial.printf("AVRCP_SOURCE_REGISTER_PLAY_STATUS requested=%u\n",
        bluetooth.avrcp().registerNotifications(
          EspBleClassicAvrcpNotification::PlayStatus) ? 1 : 0);
    else if (command == "y")
      // Repeat mode (attribute 2) set to "single track" (value 2). A Target may
      // refuse it; the command being accepted locally is all this reports.
      Serial.printf("AVRCP_SOURCE_PLAYER_SETTING requested=%u\n",
        bluetooth.avrcp().setPlayerSetting(2, 2) ? 1 : 0);
#if defined(ESPBLE_TEST_DUAL_A2DP)
    else if (command == "r")
      Serial.printf("DUAL_A2DP_BLE_READ_REQUESTED %u\n",
        dualConnectionId != 0 && dualBle.readCharacteristic(
          dualConnectionId, DualA2dpServiceUuid,
          DualA2dpCharacteristicUuid) ? 1 : 0);
#endif
  }
  if (bluetooth.a2dpSource().streaming() && sentPackets < packetTarget)
  {
    for (size_t attempt = 0; attempt < 32 && sentPackets < packetTarget; ++attempt)
      if (sendPacket() != EspBleClassicAudioSendResult::Accepted) break;
    if (sentPackets == packetTarget)
#if defined(ESPBLE_TEST_DUAL_A2DP)
      drainDeadline = millis() + 3000;
#else
      drainDeadline = millis() + 1000;
#endif
  }
  if (bluetooth.a2dpSource().streaming() &&
      sentPackets == packetTarget && !suspendRequested &&
      static_cast<int32_t>(millis() - drainDeadline) >= 0)
  {
    suspendRequested = true;
    Serial.printf("A2DP_SOURCE_SUSPEND requested=%u\n",
      bluetooth.a2dpSource().suspend() ? 1 : 0);
  }
  if (teardownRequested)
  {
    teardownRequested = false;
    Serial.printf(
      "A2DP_SOURCE_HEAP baseline=%lu current=%lu minimum=%lu largest=%lu\n",
      static_cast<unsigned long>(baselineHeap),
      static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>(
        heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)),
      static_cast<unsigned long>(
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    bluetooth.avrcp().end();
    bluetooth.a2dpSource().end();
    Serial.println("AVRCP_SOURCE_ENDED");
  }
  delay(1);
}
