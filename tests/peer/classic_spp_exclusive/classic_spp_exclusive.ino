#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sessionId = 0;

void printAddress()
{
  uint8_t address[6] = {};
  if (esp_read_mac(address, ESP_MAC_BT) != ESP_OK)
  {
    Serial.println("CLASSIC_ADDRESS_FAILED");
    return;
  }
  Serial.printf(
    "CLASSIC_SERVER_READY address=%02x:%02x:%02x:%02x:%02x:%02x heap=%u\n",
    address[0], address[1], address[2], address[3], address[4], address[5],
    static_cast<unsigned>(ESP.getFreeHeap()));
}

bool startStack()
{
  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Test";
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "CLASSIC_BEGIN_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return false;
  }
  if (!bluetooth.spp().startServer())
  {
    Serial.printf(
      "CLASSIC_SERVER_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return false;
  }
  printAddress();
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  bluetooth.spp().onServerStarted(
    [](const EspBleClassicSppServer &server) {
      Serial.printf("CLASSIC_SERVER_STARTED channel=%u\n", server.channel);
    });
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sessionId = session.id;
    Serial.printf(
      "CLASSIC_CONNECTED id=%u incoming=%u peer=%s\n",
      static_cast<unsigned>(session.id), session.incoming ? 1 : 0,
      session.peerAddress.c_str());
  });
  bluetooth.spp().onData([](const EspBleClassicSppData &event) {
    Serial.printf(
      "CLASSIC_RX id=%u length=%u hex=",
      static_cast<unsigned>(event.sessionId),
      static_cast<unsigned>(event.value.length()));
    for (size_t index = 0; index < event.value.length(); ++index)
      Serial.printf("%02x", static_cast<uint8_t>(event.value[index]));
    Serial.println();
    Serial.printf(
      "CLASSIC_ECHO_ACCEPTED %u\n",
      bluetooth.spp().write(event.sessionId, event.value) ? 1 : 0);
  });
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &session) {
    sessionId = 0;
    Serial.printf("CLASSIC_DISCONNECTED id=%u\n", static_cast<unsigned>(session.id));
  });
  startStack();
}

void loop()
{
  bluetooth.update();
  if (Serial.available())
  {
    const char command = static_cast<char>(Serial.read());
    if (command == 'r')
    {
      bluetooth.end();
      Serial.printf("CLASSIC_ENDED heap=%u\n", static_cast<unsigned>(ESP.getFreeHeap()));
      delay(100);
      startStack();
    }
    else if (command == '2')
    {
      // A second service record on the same device. Each server gets its own
      // channel, which is how a peer picks between them.
      EspBleClassicSppServerConfig second;
      second.serviceName = "EspBle SPP Second";
      Serial.printf("CLASSIC_SECOND_SERVER started=%u error=%s\n",
        bluetooth.spp().startServer(second) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 's')
      Serial.printf("CLASSIC_SERVERS_STOPPED %u\n",
        bluetooth.spp().stopServer() ? 1 : 0);
    else if (command == '1')
      Serial.printf("CLASSIC_FIRST_SERVER started=%u error=%s\n",
        bluetooth.spp().startServer() ? 1 : 0, bluetooth.lastErrorName());
    else if (command == '?')
    {
      Serial.printf("CLASSIC_SERVERS count=%u",
        static_cast<unsigned>(bluetooth.spp().serverCount()));
      for (size_t index = 0; index < bluetooth.spp().serverCount(); ++index)
      {
        EspBleClassicSppServer server;
        if (bluetooth.spp().server(index, server))
          Serial.printf(" %u:%s", server.channel, server.serviceName.c_str());
      }
      Serial.println();
    }
  }
  delay(1);
}
