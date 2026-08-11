#include <EspBleClassic.h>

EspBleClassic bluetooth;
uint32_t nextTimestamp = 1000;
size_t sentPackets = 0;
size_t wouldBlockCount = 0;
uint32_t drainDeadline = 0;
bool suspendRequested = false;
constexpr size_t PacketTarget = 100;

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.a2dpSource().onConnected(
    [](const EspBleClassicA2dpConnection &connection) {
      Serial.printf("A2DP_SOURCE_CONNECTED id=%u mtu=%u\n",
        connection.id, connection.mediaMtu);
      Serial.printf("A2DP_SOURCE_START requested=%u\n",
        bluetooth.a2dpSource().start() ? 1 : 0);
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
      Serial.printf("A2DP_SOURCE_DISCONNECTED sent=%u would_block=%u\n",
        static_cast<unsigned>(sentPackets),
        static_cast<unsigned>(wouldBlockCount));
    });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle Raw A2DP Source Peer";
  if (!bluetooth.begin(stackConfig))
  {
    Serial.printf("A2DP_SOURCE_STACK_FAILED %s:%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  const bool initialized = bluetooth.a2dpSource().begin();
  Serial.printf("A2DP_SOURCE_PROFILE initialized=%u\n",
    initialized ? 1 : 0);
  Serial.printf("A2DP_SOURCE_READY endpoint=%u seid=0\n",
    initialized ? 1 : 0);
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
    if (sentPackets <= 3 || sentPackets == PacketTarget)
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
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.startsWith("c"))
      Serial.printf("A2DP_SOURCE_CONNECT requested=%u\n",
        bluetooth.a2dpSource().connect(command.c_str() + 1) ? 1 : 0);
  }
  if (bluetooth.a2dpSource().streaming() && sentPackets < PacketTarget)
  {
    for (size_t attempt = 0; attempt < 32 && sentPackets < PacketTarget; ++attempt)
      if (sendPacket() != EspBleClassicAudioSendResult::Accepted) break;
    if (sentPackets == PacketTarget) drainDeadline = millis() + 1000;
  }
  if (bluetooth.a2dpSource().streaming() &&
      sentPackets == PacketTarget && !suspendRequested &&
      static_cast<int32_t>(millis() - drainDeadline) >= 0)
  {
    suspendRequested = true;
    Serial.printf("A2DP_SOURCE_SUSPEND requested=%u\n",
      bluetooth.a2dpSource().suspend() ? 1 : 0);
  }
  delay(1);
}
