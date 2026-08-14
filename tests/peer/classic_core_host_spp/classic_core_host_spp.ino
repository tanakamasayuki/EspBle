// EspBle's independently built Classic host, talking SPP to the Bluedroid host
// that Arduino-ESP32 ships. The peer sketch links no EspBle code at all, so a
// pass here is interoperability between two different stacks rather than two
// copies of the same one.
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sessionId = 0;

void printAddress()
{
  uint8_t address[6] = {};
  if (esp_read_mac(address, ESP_MAC_BT) != ESP_OK)
  {
    Serial.println("COREHOST_ADDRESS_FAILED");
    return;
  }
  Serial.printf(
    "COREHOST_SERVER_READY address=%02x:%02x:%02x:%02x:%02x:%02x heap=%u\n",
    address[0], address[1], address[2], address[3], address[4], address[5],
    static_cast<unsigned>(ESP.getFreeHeap()));
}

bool startStack()
{
  EspBleClassicConfig config;
  config.deviceName = "EspBle Interop";
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "COREHOST_BEGIN_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return false;
  }
  if (!bluetooth.spp().startServer())
  {
    Serial.printf(
      "COREHOST_SERVER_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return false;
  }
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.spp().onServerStarted(
    [](const EspBleClassicSppServer &server) {
      Serial.printf("COREHOST_SERVER_STARTED channel=%u\n", server.channel);
    });
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sessionId = session.id;
    Serial.printf(
      "COREHOST_CONNECTED id=%u incoming=%u peer=%s\n",
      static_cast<unsigned>(session.id), session.incoming ? 1 : 0,
      session.peerAddress.c_str());
  });
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &session) {
    if (session.id == sessionId) sessionId = 0;
    Serial.printf(
      "COREHOST_DISCONNECTED id=%u\n", static_cast<unsigned>(session.id));
  });
  bluetooth.spp().onData([](const EspBleClassicSppData &event) {
    Serial.printf("COREHOST_RX id=%u length=%u hex=",
      static_cast<unsigned>(event.sessionId),
      static_cast<unsigned>(event.value.length()));
    for (size_t i = 0; i < event.value.length(); ++i)
      Serial.printf("%02x", static_cast<uint8_t>(event.value[i]));
    Serial.println();
  });

  if (!startStack()) return;
  printAddress();
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'e')
    {
      // Echo a fixed payload that contains a zero byte, so the transfer is
      // proven binary-clean in both stacks rather than string-clean.
      const uint8_t payload[] = {0xa5, 0x00, 0x5a, 0xff};
      const bool sent = sessionId != 0 &&
        bluetooth.spp().write(sessionId, payload, sizeof(payload));
      Serial.printf("COREHOST_TX sent=%u\n", sent ? 1 : 0);
    }
    else if (command == 'd')
    {
      const bool requested = sessionId != 0 &&
        bluetooth.spp().disconnect(sessionId);
      Serial.printf("COREHOST_DISCONNECT requested=%u\n", requested ? 1 : 0);
    }
    else if (command == '?')
    {
      Serial.printf("COREHOST_STATE server=%u sessions=%u dropped=%u heap=%u\n",
        bluetooth.spp().serverRunning() ? 1 : 0,
        static_cast<unsigned>(bluetooth.spp().sessionCount()),
        static_cast<unsigned>(bluetooth.spp().droppedEventCount()),
        static_cast<unsigned>(ESP.getFreeHeap()));
    }
    else if (command == 'r')
    {
      bluetooth.end();
      sessionId = 0;
      const bool restarted = startStack();
      Serial.printf("COREHOST_RESTART started=%u\n", restarted ? 1 : 0);
      if (restarted) printAddress();
    }
  }
  delay(1);
}
