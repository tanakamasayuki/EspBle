// DUT for the core-host security interoperability test. EspBle on a NimBLE host
// pairs and bonds with peer_device/, which runs the Bluedroid-backed BLE
// wrapper Arduino-ESP32 ships. Encryption, key exchange and bond storage all
// cross the stack boundary here.
#include <EspBle.h>

static const char *ServiceUuid = "2f7a2000-9d0b-4f6a-9b41-1c8f3a5d0002";
static const char *SecureUuid = "2f7a2001-9d0b-4f6a-9b41-1c8f3a5d0002";

EspBle ble;
bool connectRequested = false;
EspBleConnectionId connectionId = 0;
String peerAddress;
bool encrypted = false;
bool bonded = false;

void reportState()
{
  Serial.printf("SECGATT_STATE connected=%u encrypted=%u bonded=%u bonds=%u\n",
    connectionId != 0 ? 1 : 0, encrypted ? 1 : 0, bonded ? 1 : 0,
    static_cast<unsigned>(ble.bondCount()));
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle CoreHost Secure Central";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.pairOnConnect = true;
  config.security.mitm = false;
  config.security.ioCapability = EspBleSecurityIoCapability::None;
  if (!ble.begin(config))
  {
    Serial.printf("SECGATT_INIT_FAILED %s:%s\n", ble.lastErrorName(),
      ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectRequested || !result.advertisesService(ServiceUuid)) return;
    ble.scanner().stop();
    peerAddress = result.address;
    connectRequested = ble.connect(result);
    Serial.printf("SECGATT_CONNECT requested=%u peer=%s\n",
      connectRequested ? 1 : 0, peerAddress.c_str());
  });

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    encrypted = connection.encrypted;
    bonded = connection.bonded;
    Serial.printf("SECGATT_CONNECTED id=%u encrypted=%u bonded=%u\n",
      connection.id, connection.encrypted ? 1 : 0, connection.bonded ? 1 : 0);
  });

  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    encrypted = event.connection.encrypted;
    bonded = event.connection.bonded;
    Serial.printf("SECGATT_SECURITY success=%u encrypted=%u bonded=%u detail=%s\n",
      event.success ? 1 : 0, event.connection.encrypted ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      event.detail.length() != 0 ? event.detail.c_str() : "none");
  });

  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    // A rejected read must not look like a successful empty read, so the ATT
    // error the peer returned is printed as the backend reported it.
    Serial.printf("SECGATT_READ success=%u value=%s detail=%s\n",
      result.success ? 1 : 0, result.value.c_str(),
      result.detail.length() != 0 ? result.detail.c_str() : "none");
  });

  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    encrypted = false;
    bonded = false;
    Serial.printf("SECGATT_DISCONNECTED reason=%u\n", connection.disconnectReason);
  });

  Serial.println("SECGATT_READY");
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "?")
    {
      Serial.println("SECGATT_READY");
      reportState();
    }
    else if (command == "s")
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.printf("SECGATT_SCAN started=%u\n",
        ble.scanner().start(scanConfig) ? 1 : 0);
    }
    else if (command == "r" && connectionId != 0)
    {
      Serial.printf("SECGATT_READ_REQUESTED %u\n",
        ble.readCharacteristic(connectionId, ServiceUuid, SecureUuid) ? 1 : 0);
    }
    else if (command == "p" && connectionId != 0)
    {
      Serial.printf("SECGATT_PAIR_REQUESTED %u\n",
        ble.requestSecurity(connectionId) ? 1 : 0);
    }
    else if (command == "c")
    {
      // Bond removal on this side goes through the public API, so the peer's
      // stored keys become the only ones left and the next pairing is fresh.
      unsigned removed = 0;
      while (ble.bondCount() != 0)
      {
        EspBleBond bond;
        if (!ble.bond(0, bond)) break;
        if (!ble.deleteBond(bond)) break;
        ++removed;
      }
      Serial.printf("SECGATT_BONDS_CLEARED removed=%u remaining=%u\n", removed,
        static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == "x" && connectionId != 0)
    {
      Serial.printf("SECGATT_DISCONNECT_REQUESTED %u\n",
        ble.disconnect(connectionId) ? 1 : 0);
    }
    else if (command == "a")
    {
      connectRequested = false;
      Serial.println("SECGATT_REARMED");
    }
  }

  ble.update();
  delay(1);
}
