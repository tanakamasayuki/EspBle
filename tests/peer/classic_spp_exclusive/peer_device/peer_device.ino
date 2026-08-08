#include <EspBleClassic.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sessionId = 0;
String command;

void setup()
{
  Serial.begin(115200);
  delay(500);
  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Peer";
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "CLASSIC_PEER_BEGIN_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sessionId = session.id;
    Serial.printf(
      "CLASSIC_PEER_CONNECTED id=%u incoming=%u peer=%s\n",
      static_cast<unsigned>(session.id), session.incoming ? 1 : 0,
      session.peerAddress.c_str());
    const uint8_t payload[] = {0x00, 0x7f, 0x80, 0xff, 'S', 'P', 'P'};
    Serial.printf(
      "CLASSIC_PEER_WRITE_ACCEPTED %u\n",
      bluetooth.spp().write(session.id, payload, sizeof(payload)) ? 1 : 0);
  });
  bluetooth.spp().onData([](const EspBleClassicSppData &event) {
    Serial.printf(
      "CLASSIC_PEER_ECHO id=%u length=%u hex=",
      static_cast<unsigned>(event.sessionId),
      static_cast<unsigned>(event.value.length()));
    for (size_t index = 0; index < event.value.length(); ++index)
      Serial.printf("%02x", static_cast<uint8_t>(event.value[index]));
    Serial.println();
  });
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &session) {
    sessionId = 0;
    Serial.printf("CLASSIC_PEER_DISCONNECTED id=%u\n", static_cast<unsigned>(session.id));
  });
  bluetooth.spp().onConnectionFailed([](const EspBleClassicSppConnectionFailure &failure) {
    Serial.printf(
      "CLASSIC_PEER_CONNECT_FAILED peer=%s error=%u detail=%s\n",
      failure.peerAddress.c_str(), static_cast<unsigned>(failure.error),
      failure.detail.c_str());
  });
  Serial.println("CLASSIC_PEER_READY");
}

void handleCommand(const String &value)
{
  if (value.startsWith("c"))
  {
    Serial.printf(
      "CLASSIC_PEER_CONNECT_ACCEPTED %u\n",
      bluetooth.spp().connect(value.c_str() + 1, 15000) ? 1 : 0);
  }
  else if (value == "d" && sessionId != 0)
  {
    Serial.printf(
      "CLASSIC_PEER_DISCONNECT_ACCEPTED %u\n",
      bluetooth.spp().disconnect(sessionId) ? 1 : 0);
  }
}

void loop()
{
  bluetooth.update();
  while (Serial.available())
  {
    const char value = static_cast<char>(Serial.read());
    if (value == '\n' || value == '\r')
    {
      if (!command.isEmpty()) handleCommand(command);
      command = "";
    }
    else
    {
      command += value;
    }
  }
  delay(1);
}
