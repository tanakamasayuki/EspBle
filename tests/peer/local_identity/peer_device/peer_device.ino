// Peripheral for the local_identity peer test: report its own address, change
// its transmit power on command, and disconnect with a chosen reason code.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAE";

EspBle ble;
EspBleConnectionId activeConnection = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Local Identity";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    activeConnection = connection.id;
    Serial.printf("PERIPHERAL_CONNECTED id=%u\n", connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    activeConnection = 0;
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u\n", connection.id);
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Local Identity");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.data().setTxPowerIncluded(true);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

static void restartAdvertising()
{
  ble.advertising().stop();
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'a')
    {
      // The address this device presents right now.
      Serial.printf(
        "LOCAL_ADDRESS %s type=%u\n",
        ble.localAddress().c_str(),
        static_cast<unsigned>(ble.localAddressType()));
    }
    else if (command == 'l' || command == 'h')
    {
      // Ask for a low / high level; the radio applies the nearest it supports.
      Serial.printf("SET_TX_POWER %d\n", ble.setTxPower(command == 'l' ? -12 : 9) ? 1 : 0);
      Serial.printf("TX_POWER %d\n", static_cast<int>(ble.txPower()));
      restartAdvertising();
    }
    else if (command == 'd')
    {
      // 0x16 = connection terminated by local host.
      Serial.printf("DISCONNECT %d\n", ble.disconnect(activeConnection, 0x16) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
