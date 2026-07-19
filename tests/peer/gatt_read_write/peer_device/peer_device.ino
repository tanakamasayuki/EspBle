#include <EspBle.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";
static constexpr const char *TEST_DESCRIPTOR_UUID = "10da4dd2-8eaa-4c69-9003-676174747277";
static constexpr const char *SLOW_SERVICE_UUID = "10da4de0-8eaa-4c69-9003-676174747277";
static constexpr const char *SLOW_CHARACTERISTIC_UUID = "10da4de1-8eaa-4c69-9003-676174747277";

EspBle ble;
TaskHandle_t loopTask = nullptr;

class SlowReadCallbacks : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *) override
  {
    delay(1000);
  }
};

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.writable = true;
  characteristicConfig.writableWithoutResponse = true;
  EspBleGattDescriptorConfig descriptorConfig;
  descriptorConfig.readable = true;
  descriptorConfig.writable = true;
  if (!gattServer.addService(TEST_SERVICE_UUID) ||
      !gattServer.addCharacteristic(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, characteristicConfig) ||
      !gattServer.addDescriptor(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, TEST_DESCRIPTOR_UUID, descriptorConfig) ||
      !gattServer.setValue(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("peer-ready")) ||
      !gattServer.setDescriptorValue(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID,
        TEST_DESCRIPTOR_UUID, String("peer-description")))
  {
    Serial.printf("GATT_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  gattServer.onWritten([](const EspBleGattWrite &write) {
    String storedValue;
    const bool stored = ble.gattServer().value(
      TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, storedValue);
    Serial.printf(
      "SERVER_WRITE id=%u identified=%u value=%s stored=%u context=%s\n",
      static_cast<unsigned>(write.connectionId),
      write.connectionIdentified ? 1 : 0,
      write.value.c_str(),
      stored && storedValue == write.value ? 1 : 0,
      xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack");
  });
  gattServer.onDescriptorWritten([](const EspBleGattWrite &write) {
    String storedValue;
    const bool stored = ble.gattServer().descriptorValue(
      write.serviceUuid.c_str(),
      write.characteristicUuid.c_str(),
      write.descriptorUuid.c_str(),
      storedValue);
    Serial.printf(
      "SERVER_DESCRIPTOR_WRITE id=%u identified=%u value=%s stored=%u context=%s\n",
      static_cast<unsigned>(write.connectionId),
      write.connectionIdentified ? 1 : 0,
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

  // A deliberately slow backend characteristic verifies client operation
  // timeout and late-completion suppression without changing the public
  // EspBle GATT Server API solely for test instrumentation.
  BLEService *slowService = BLEDevice::getServer()->createService(SLOW_SERVICE_UUID);
  BLECharacteristic *slowCharacteristic = slowService->createCharacteristic(
    SLOW_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  slowCharacteristic->setValue("slow-ready");
  slowCharacteristic->setCallbacks(new SlowReadCallbacks());
  slowService->start();

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
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'd')
    {
      String value;
      const bool found = ble.gattServer().descriptorValue(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, TEST_DESCRIPTOR_UUID, value);
      Serial.printf("SERVER_DESCRIPTOR found=%u value=%s\n",
        found ? 1 : 0, value.c_str());
    }
  }

  ble.update();
  delay(1);
}
