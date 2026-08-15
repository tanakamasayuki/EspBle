#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sessionId = 0;
String pendingComparisonAddress;
uint32_t pendingComparisonValue;
bool autoAccept = true;

void reportBonds(const char *prefix)
{
  Serial.printf("%s count=%u", prefix,
    static_cast<unsigned>(bluetooth.bondCount()));
  for (size_t index = 0; index < bluetooth.bondCount(); ++index)
  {
    EspBleClassicBond bond;
    if (bluetooth.bond(index, bond))
      Serial.printf(" %s", bond.peerAddress.c_str());
  }
  Serial.println();
}

bool startStack()
{
  EspBleClassicConfig config;
  config.deviceName = "EspBle Pairing Test";
  config.security.enabled = true;
  config.security.ioCapability = EspBleClassicSecurityIoCapability::DisplayYesNo;
  config.security.responseTimeoutMilliseconds = 5000;
  if (!bluetooth.begin(config))
  {
    Serial.printf("PAIR_BEGIN_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return false;
  }
  if (!bluetooth.spp().startServer())
  {
    Serial.printf("PAIR_SERVER_FAILED error=%s\n", bluetooth.lastErrorName());
    return false;
  }
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.onNumericComparisonRequested(
    [](const EspBleClassicNumericComparison &event) {
      pendingComparisonAddress = event.peerAddress;
      pendingComparisonValue = event.value;
      Serial.printf("PAIR_COMPARE peer=%s value=%06u auto=%u\n",
        event.peerAddress.c_str(), static_cast<unsigned>(event.value),
        autoAccept ? 1 : 0);
      if (autoAccept)
        Serial.printf("PAIR_CONFIRM accepted=%u\n",
          bluetooth.confirmNumericComparison(event.peerAddress.c_str(), true)
            ? 1 : 0);
    });
  bluetooth.onSecurityChanged([](const EspBleClassicSecurityChanged &event) {
    Serial.printf("PAIR_SECURITY peer=%s success=%u status=%d\n",
      event.peerAddress.c_str(), event.success ? 1 : 0, event.status);
  });
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sessionId = session.id;
    Serial.printf("PAIR_CONNECTED id=%u peer=%s\n",
      static_cast<unsigned>(session.id), session.peerAddress.c_str());
  });
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &session) {
    if (session.id == sessionId) sessionId = 0;
    Serial.printf("PAIR_DISCONNECTED id=%u\n",
      static_cast<unsigned>(session.id));
  });

  if (!startStack()) return;
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf("PAIR_READY address=%02x:%02x:%02x:%02x:%02x:%02x\n",
    address[0], address[1], address[2], address[3], address[4], address[5]);
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'b') reportBonds("PAIR_BONDS");
    else if (command == 'n')
    {
      autoAccept = false;
      Serial.println("PAIR_AUTO off");
    }
    else if (command == 'y')
    {
      autoAccept = true;
      Serial.println("PAIR_AUTO on");
    }
    else if (command == 'a')
    {
      Serial.printf("PAIR_ANSWER accepted=%u error=%s\n",
        bluetooth.confirmNumericComparison(
          pendingComparisonAddress.c_str(), true) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 'r')
    {
      Serial.printf("PAIR_REJECT rejected=%u error=%s\n",
        bluetooth.confirmNumericComparison(
          pendingComparisonAddress.c_str(), false) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 'w')
    {
      // Answering when nothing is waiting must fail rather than reply for a
      // pairing that already ended.
      Serial.printf("PAIR_STALE accepted=%u error=%s\n",
        bluetooth.confirmNumericComparison("00:00:00:00:00:00", true) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 'x')
    {
      Serial.printf("PAIR_DELETE_ALL removed=%u count=%u\n",
        bluetooth.deleteAllBonds() ? 1 : 0,
        static_cast<unsigned>(bluetooth.bondCount()));
    }
    else if (command == 'd')
    {
      Serial.printf("PAIR_DISCONNECT requested=%u\n",
        sessionId != 0 && bluetooth.spp().disconnect(sessionId) ? 1 : 0);
    }
    else if (command == '?')
    {
      Serial.printf("PAIR_STATE sessions=%u bonds=%u heap=%u\n",
        static_cast<unsigned>(bluetooth.spp().sessionCount()),
        static_cast<unsigned>(bluetooth.bondCount()),
        static_cast<unsigned>(ESP.getFreeHeap()));
    }
  }
  delay(1);
}
