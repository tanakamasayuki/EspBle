// Peripheral for the accept_list peer test: advertise connectably, switching
// between an accept-list-restricted filter policy (only a bogus address is
// listed, so nobody may connect) and the default open policy.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAD";
// An address no board will ever present, so a restricted policy rejects every
// connection request without needing to know the central's own address.
static constexpr const char *UNREACHABLE_PEER = "00:00:00:00:00:01";

EspBle ble;

static void restartAdvertising(EspBleAdvertisingFilterPolicy policy, const char *label)
{
  ble.advertising().stop();
  ble.advertising().setFilterPolicy(policy);
  if (!ble.advertising().start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.printf("POLICY %s entries=%u\n", label, static_cast<unsigned>(ble.acceptListCount()));
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Accept List";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  if (!ble.addToAcceptList(UNREACHABLE_PEER, EspBleAddressType::Public))
  {
    Serial.printf("ACCEPT_LIST_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_CONNECTED id=%u\n", connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u\n", connection.id);
  });

  ble.advertising().setName("EspBle Accept List");
  ble.advertising().addServiceUuid(SERVICE_UUID);
  restartAdvertising(EspBleAdvertisingFilterPolicy::ConnectionFromAcceptList, "restricted");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'r')
    {
      restartAdvertising(EspBleAdvertisingFilterPolicy::ConnectionFromAcceptList, "restricted");
    }
    else if (command == 'o')
    {
      restartAdvertising(EspBleAdvertisingFilterPolicy::Any, "open");
    }
    else if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
