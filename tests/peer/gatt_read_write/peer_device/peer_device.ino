#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";
static constexpr const char *TEST_DESCRIPTOR_UUID = "10da4dd2-8eaa-4c69-9003-676174747277";
static constexpr const char *SLOW_SERVICE_UUID = "10da4de0-8eaa-4c69-9003-676174747277";
static constexpr const char *SLOW_CHARACTERISTIC_UUID = "10da4de1-8eaa-4c69-9003-676174747277";

EspBle ble;
EspBleGattService testServiceService;
EspBleGattCharacteristic testCharacteristicCharacteristic;
EspBleGattDescriptor testDescriptorDescriptor;
EspBleGattService slowServiceService;
EspBleGattCharacteristic slowCharacteristicCharacteristic;
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
  characteristicConfig.writableWithoutResponse = true;
  EspBleGattDescriptorConfig descriptorConfig;
  descriptorConfig.readable = true;
  descriptorConfig.writable = true;
  if (!(testServiceService = gattServer.addService(TEST_SERVICE_UUID)).valid() ||
      !(testCharacteristicCharacteristic = gattServer.addCharacteristic(testServiceService, TEST_CHARACTERISTIC_UUID, characteristicConfig)).valid() ||
      !(testDescriptorDescriptor = gattServer.addDescriptor(testCharacteristicCharacteristic, TEST_DESCRIPTOR_UUID, descriptorConfig)).valid() ||
      !gattServer.setValue(testCharacteristicCharacteristic, String("peer-ready")) ||
      !gattServer.setDescriptorValue(testDescriptorDescriptor, String("peer-description")))
  {
    Serial.printf("GATT_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  EspBleGattCharacteristicConfig slowConfig;
  slowConfig.readable = true;
  if (!(slowServiceService = gattServer.addService(SLOW_SERVICE_UUID)).valid() ||
      !(slowCharacteristicCharacteristic = gattServer.addCharacteristic(
          slowServiceService, SLOW_CHARACTERISTIC_UUID, slowConfig)).valid() ||
      !gattServer.setValue(slowCharacteristicCharacteristic, String("slow-ready")))
  {
    Serial.printf("GATT_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  gattServer.onWritten([](const EspBleGattWrite &write) {
    String storedValue;
    const bool stored = ble.gattServer().value(testCharacteristicCharacteristic, storedValue);
    Serial.printf(
      "SERVER_WRITE id=%u value=%s stored=%u context=%s\n",
      static_cast<unsigned>(write.connectionId),
      write.value.c_str(),
      stored && storedValue == write.value ? 1 : 0,
      xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack");
  });
  gattServer.onDescriptorWritten([](const EspBleGattDescriptorWrite &write) {
    String storedValue;
    // The event carries the handle of the descriptor that was written, so the
    // value can be read back without matching UUIDs.
    const bool stored = ble.gattServer().descriptorValue(write.descriptor, storedValue);
    Serial.printf(
      "SERVER_DESCRIPTOR_WRITE value=%s stored=%u context=%s\n",
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

  // A deliberately slow read verifies the client's operation timeout and its
  // suppression of a late completion. onRead() runs on the stack task, so the
  // delay here is exactly the "anything slow stalls the stack" case its
  // documentation warns about -- which is the point: the reader must see the
  // read take a second.
  gattServer.onRead([](const EspBleGattReadRequest &request) {
    if (request.characteristic == slowCharacteristicCharacteristic) delay(1000);
  });

  // Advertising does not restart by itself after a disconnect, so the
  // reconnect-cycle test would find nothing on its second pass.
  ble.onDisconnected([](const EspBleConnection &) {
    Serial.printf("DEVICE_READVERTISING %u\n", ble.advertising().start() ? 1 : 0);
  });

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
      const bool found = ble.gattServer().descriptorValue(testDescriptorDescriptor, value);
      Serial.printf("SERVER_DESCRIPTOR found=%u value=%s\n",
        found ? 1 : 0, value.c_str());
    }
  }

  ble.update();
  delay(1);
}
