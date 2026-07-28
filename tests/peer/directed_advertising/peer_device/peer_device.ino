#include <EspBle.h>

static constexpr const char *TEST_SERVICE_UUID = "3d9b1c40-6f2e-4a8b-9f31-64697265637a";

EspBle ble;
String centralAddress;
EspBleAddressType centralAddressType = EspBleAddressType::Public;

static void startUndirected()
{
  auto &advertising = ble.advertising();
  advertising.clearDirectedTarget();
  advertising.setName("EspBle Directed Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  Serial.printf("ADVERTISING %u\n", advertising.start() ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Directed Peer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    centralAddress = connection.peerAddress;
    centralAddressType = connection.peerAddressType;
    Serial.printf("PERIPHERAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });

  startUndirected();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    auto &advertising = ble.advertising();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", advertising.isAdvertising() ? 1 : 0);
    }
    else if (command == 'u')
    {
      advertising.stop();
      advertising.setChannelMap(0);
      startUndirected();
    }
    else if (command == 'D')
    {
      advertising.stop();
      const bool targeted =
        advertising.setDirectedTarget(centralAddress.c_str(), centralAddressType);
      Serial.printf("DIRECTED_TARGET success=%u\n", targeted ? 1 : 0);
      Serial.printf("ADVERTISING %u\n", advertising.start() ? 1 : 0);
    }
    else if (command == 'm')
    {
      // Channel 39 only: advertising must still work, just on one channel.
      advertising.stop();
      advertising.clearDirectedTarget();
      const bool mapped = advertising.setChannelMap(EspBleAdvertisingChannel39);
      Serial.printf("CHANNEL_MAP success=%u\n", mapped ? 1 : 0);
      Serial.printf("ADVERTISING %u\n", advertising.start() ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
