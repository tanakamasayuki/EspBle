// SPP client using the session API, so the Stream adapter under test on the
// other board is measured against the plain API rather than against itself.
// Received bytes are counted and folded into an Adler-style checksum, which is
// order-sensitive: a split buffer reassembled out of order changes it.
#include <EspBleClassic.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sessionId = 0;
uint32_t receivedBytes = 0;
uint32_t checksumA = 1;
uint32_t checksumB = 0;
String lineBuffer;

void consume(uint8_t value)
{
  ++receivedBytes;
  checksumA = (checksumA + value) % 65521u;
  checksumB = (checksumB + checksumA) % 65521u;
  if (lineBuffer.length() < 120 && value != '\n' && value != '\r')
    lineBuffer += static_cast<char>(value);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sessionId = session.id;
    Serial.printf("PEER_CONNECTED peer=%s\n", session.peerAddress.c_str());
  });
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &session) {
    if (session.id == sessionId) sessionId = 0;
    Serial.println("PEER_DISCONNECTED");
  });
  bluetooth.spp().onConnectionFailed(
    [](const EspBleClassicSppConnectionFailure &failure) {
      Serial.printf("PEER_FAILED peer=%s detail=%s\n",
        failure.peerAddress.c_str(), failure.detail.c_str());
    });
  bluetooth.spp().onData([](const EspBleClassicSppData &event) {
    for (size_t index = 0; index < event.value.length(); ++index)
      consume(static_cast<uint8_t>(event.value[index]));
  });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Stream Peer";
  if (!bluetooth.begin(config))
  {
    Serial.printf("PEER_BEGIN_FAILED error=%s\n", bluetooth.lastErrorName());
    return;
  }
  Serial.println("PEER_READY");
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const String line = Serial.readStringUntil('\n');
    if (line.length() == 0) return;
    const char command = line[0];
    if (command == 'c')
      Serial.printf("PEER_CONNECT requested=%u\n",
        bluetooth.spp().connect(line.substring(1).c_str()) ? 1 : 0);
    else if (command == 'w' && sessionId != 0)
    {
      String payload = line.substring(1);
      payload += "\n";
      Serial.printf("PEER_WROTE accepted=%u len=%u\n",
        bluetooth.spp().write(sessionId, payload) ? 1 : 0,
        static_cast<unsigned>(payload.length()));
    }
    else if (command == 'z')
    {
      receivedBytes = 0;
      checksumA = 1;
      checksumB = 0;
      lineBuffer = "";
      Serial.println("PEER_RESET");
    }
    else if (command == '?')
      Serial.printf("PEER_STATE session=%u bytes=%u checksum=%u line=%s\n",
        static_cast<unsigned>(sessionId), static_cast<unsigned>(receivedBytes),
        static_cast<unsigned>((checksumB << 16) | checksumA),
        lineBuffer.c_str());
    else if (command == 'd' && sessionId != 0)
      bluetooth.spp().disconnect(sessionId);
  }
  delay(1);
}
