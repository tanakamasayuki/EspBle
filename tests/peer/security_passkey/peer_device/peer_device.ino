#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "35c6a570-a63d-44a2-9003-706173736b79";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "35c6a571-a63d-44a2-9003-706173736b79";
static constexpr uint32_t TEST_PASSKEY = 438209;

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.writable = true;
  characteristicConfig.authenticatedRead = true;
  characteristicConfig.authenticatedWrite = true;
  auto &gattServer = ble.gattServer();
  gattServer.addService(TEST_SERVICE_UUID);
  gattServer.addCharacteristic(TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, characteristicConfig);
  gattServer.setValue(TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("authenticated-ready"));
  gattServer.onWritten([](const EspBleGattWrite &write) {
    EspBleConnection connection;
    const bool found = ble.connection(write.connectionId, connection);
    Serial.printf(
      "AUTHENTICATED_WRITE value=%s authenticated=%u context=%s\n",
      write.value.c_str(),
      found && connection.authenticated ? 1 : 0,
      callbackContext());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Passkey Peer";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  config.security.ioCapability = EspBleSecurityIoCapability::DisplayOnly;
  config.security.staticPasskeyEnabled = true;
  config.security.staticPasskey = TEST_PASSKEY;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onPasskeyDisplayed([](const EspBlePasskeyDisplayed &event) {
    Serial.printf(
      "PASSKEY_DISPLAYED id=%u passkey=%06u context=%s\n",
      static_cast<unsigned>(event.connection.id),
      static_cast<unsigned>(event.passkey),
      callbackContext());
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "PERIPHERAL_SECURITY success=%u encrypted=%u authenticated=%u bonded=%u key=%u context=%s\n",
      event.success ? 1 : 0,
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      event.connection.encryptionKeySize,
      callbackContext());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), callbackContext());
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Passkey Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  advertising.start();
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
    else if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
