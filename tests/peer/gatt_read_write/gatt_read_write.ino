#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";

EspBle ble;
TaskHandle_t loopTask = nullptr;
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
  config.deviceName = "EspBle GATT Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    const bool accepted = ble.discoverCharacteristic(
      connection.id, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID);
    Serial.println(accepted ? "DISCOVER_REQUESTED" : "DISCOVER_REQUEST_FAILED");
  });
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    Serial.printf(
      "DISCOVER success=%u read=%u write=%u context=%s\n",
      result.success ? 1 : 0,
      result.readable ? 1 : 0,
      result.writable ? 1 : 0,
      callbackContext());
    if (result.success)
    {
      Serial.println(
        ble.readCharacteristic(result.connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID)
          ? "READ_REQUESTED"
          : "READ_REQUEST_FAILED");
    }
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf(
      "READ success=%u value=%s context=%s\n",
      result.success ? 1 : 0,
      result.value.c_str(),
      callbackContext());
    if (result.success)
    {
      Serial.println(
        ble.writeCharacteristic(
          result.connectionId,
          TEST_SERVICE_UUID,
          TEST_CHARACTERISTIC_UUID,
          String("central-write"),
          true)
          ? "WRITE_REQUESTED"
          : "WRITE_REQUEST_FAILED");
    }
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    Serial.printf(
      "WRITE success=%u context=%s\n",
      result.success ? 1 : 0,
      callbackContext());
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
  if (Serial.available() > 0 && Serial.read() == 's' && !connectionRequested)
  {
    EspBleScanConfig scanConfig;
    scanConfig.active = true;
    Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
  }

  ble.update();
  delay(1);
}
