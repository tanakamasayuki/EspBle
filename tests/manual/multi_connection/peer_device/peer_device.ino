// Peripheral A for the multi_connection manual test.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *SERVICE_UUID = "6d756c74-6900-4100-9003-636f6e6e4141";
static constexpr const char *CHAR_UUID = "6d756c74-6900-4100-9003-636f6e6e4142";

EspBle ble;
EspBleConnectionId centralId = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.notifiable = true;
  if (!gattServer.addService(SERVICE_UUID) ||
      !gattServer.addCharacteristic(SERVICE_UUID, CHAR_UUID, characteristicConfig))
  {
    Serial.printf("GATT_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Multi Peer A";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    centralId = connection.id;
    Serial.println("PEER_A_CONNECTED");
  });
  ble.onDisconnected([](const EspBleConnection &) {
    centralId = 0;
    ble.advertising().start();
    Serial.println("PEER_A_READVERTISING");
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Multi Peer A");
  advertising.addServiceUuid(SERVICE_UUID);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'n')
    {
      Serial.println(
        ble.gattServer().notify(SERVICE_UUID, CHAR_UUID, String("A-notify"))
          ? "NOTIFY_REQUESTED" : "NOTIFY_REQUEST_FAILED");
    }
    else if (command == 'x' && centralId != 0)
    {
      // Drop the link from the peripheral side: an unexpected disconnect for the
      // central, which then auto-reconnects.
      Serial.println(ble.disconnect(centralId) ? "PEER_A_DROP_REQUESTED" : "PEER_A_DROP_FAILED");
    }
  }

  ble.update();
  delay(1);
}
