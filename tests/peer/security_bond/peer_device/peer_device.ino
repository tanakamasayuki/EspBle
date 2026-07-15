#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "e20ab920-8f4a-4e1d-9003-736563757269";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "e20ab921-8f4a-4e1d-9003-736563757269";

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static bool startAdvertising()
{
  return ble.advertising().start();
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.writable = true;
  characteristicConfig.encryptedRead = true;
  characteristicConfig.encryptedWrite = true;
  gattServer.addService(TEST_SERVICE_UUID);
  gattServer.addCharacteristic(TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, characteristicConfig);
  gattServer.setValue(TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("secure-ready"));
  gattServer.onWritten([](const EspBleGattWrite &write) {
    EspBleConnection connection;
    const bool found = ble.connection(write.connectionId, connection);
    Serial.printf(
      "SECURE_WRITE value=%s encrypted=%u bonded=%u context=%s\n",
      write.value.c_str(),
      found && connection.encrypted ? 1 : 0,
      found && connection.bonded ? 1 : 0,
      callbackContext());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Security Peer";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.pairOnConnect = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    EspBleConnection storedConnection;
    const bool stored = ble.connection(event.connection.id, storedConnection);
    Serial.printf(
      "PERIPHERAL_SECURITY success=%u encrypted=%u authenticated=%u bonded=%u key=%u stored=%u context=%s\n",
      event.success ? 1 : 0,
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      event.connection.encryptionKeySize,
      stored && storedConnection.encrypted && storedConnection.bonded ? 1 : 0,
      callbackContext());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), callbackContext());
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Security Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  startAdvertising();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("PERIPHERAL_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 'b')
    {
      Serial.printf("PERIPHERAL_BONDS count=%u\n", static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 'a')
    {
      Serial.printf("ADVERTISING %u\n", startAdvertising() ? 1 : 0);
    }
    else if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
