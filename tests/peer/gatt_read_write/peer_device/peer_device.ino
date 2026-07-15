#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";

EspBle ble;
TaskHandle_t loopTask = nullptr;

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.writable = true;
  if (!gattServer.addService(TEST_SERVICE_UUID) ||
      !gattServer.addCharacteristic(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, characteristicConfig) ||
      !gattServer.setValue(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("peer-ready")))
  {
    Serial.printf("GATT_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  gattServer.onWritten([](const EspBleGattWrite &write) {
    String storedValue;
    const bool stored = ble.gattServer().value(
      TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, storedValue);
    Serial.printf(
      "SERVER_WRITE id=%u value=%s stored=%u context=%s\n",
      static_cast<unsigned>(write.connectionId),
      write.value.c_str(),
      stored && storedValue == write.value ? 1 : 0,
      xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack");
  });

  EspBleConfig config;
  config.deviceName = "EspBle GATT Peer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle GATT Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == '?')
  {
    Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
  }

  ble.update();
  delay(1);
}
