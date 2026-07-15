#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "35c6a570-a63d-44a2-9003-706173736b79";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "35c6a571-a63d-44a2-9003-706173736b79";
static constexpr uint32_t TEST_PASSKEY = 438209;

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleConfig config;
  config.deviceName = "EspBle Passkey Central";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  config.security.ioCapability = EspBleSecurityIoCapability::KeyboardOnly;
  config.security.staticPasskeyEnabled = true;
  config.security.staticPasskey = TEST_PASSKEY;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("CENTRAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "CENTRAL_SECURITY success=%u encrypted=%u authenticated=%u bonded=%u key=%u context=%s\n",
      event.success ? 1 : 0,
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      event.connection.encryptionKeySize,
      callbackContext());
    if (event.success)
    {
      Serial.println(
        ble.discoverCharacteristic(
          event.connection.id, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID)
          ? "DISCOVER_REQUESTED"
          : "DISCOVER_REQUEST_FAILED");
    }
  });
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    Serial.printf("DISCOVER success=%u context=%s\n", result.success ? 1 : 0, callbackContext());
    if (result.success)
    {
      Serial.println(
        ble.readCharacteristic(result.connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID)
          ? "READ_REQUESTED"
          : "READ_REQUEST_FAILED");
    }
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf("READ success=%u value=%s context=%s\n",
      result.success ? 1 : 0, result.value.c_str(), callbackContext());
    if (result.success)
    {
      Serial.println(
        ble.writeCharacteristic(
          result.connectionId,
          TEST_SERVICE_UUID,
          TEST_CHARACTERISTIC_UUID,
          String("authenticated-write"),
          true)
          ? "WRITE_REQUESTED"
          : "WRITE_REQUEST_FAILED");
    }
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    Serial.printf("WRITE success=%u context=%s\n", result.success ? 1 : 0, callbackContext());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), callbackContext());
    connectionId = 0;
    connectionRequested = false;
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(TEST_SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
    Serial.println(connectionRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("CENTRAL_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 'b')
    {
      Serial.printf("CENTRAL_BONDS count=%u\n", static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 's' && !connectionRequested)
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }

  ble.update();
  delay(1);
}
