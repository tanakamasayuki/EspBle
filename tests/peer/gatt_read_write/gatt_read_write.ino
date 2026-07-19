#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";
static constexpr const char *TEST_DESCRIPTOR_UUID = "10da4dd2-8eaa-4c69-9003-676174747277";

EspBle ble;
TaskHandle_t loopTask = nullptr;
bool connectionRequested = false;
unsigned writePhase = 0;
EspBleConnectionId connectionId = 0;

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
    connectionId = connection.id;
    const bool accepted = ble.discoverServices(connection.id);
    Serial.println(accepted ? "DATABASE_REQUESTED" : "DATABASE_REQUEST_FAILED");
  });
  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    const size_t serviceCount = ble.discoveredServiceCount(result.connectionId);
    const size_t characteristicCount =
      ble.discoveredCharacteristicCount(result.connectionId, TEST_SERVICE_UUID);
    const size_t descriptorCount = ble.discoveredDescriptorCount(
      result.connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID);
    bool characteristicFound = false;
    EspBleGattCharacteristicInfo characteristic;
    for (size_t index = 0; index < characteristicCount; ++index)
    {
      EspBleGattCharacteristicInfo candidate;
      if (ble.discoveredCharacteristic(
            result.connectionId, index, candidate, TEST_SERVICE_UUID) &&
          candidate.characteristicUuid.equalsIgnoreCase(TEST_CHARACTERISTIC_UUID))
      {
        characteristic = candidate;
        characteristicFound = true;
      }
    }
    bool descriptorFound = false;
    EspBleGattDescriptorInfo descriptor;
    for (size_t index = 0; index < descriptorCount; ++index)
    {
      EspBleGattDescriptorInfo candidate;
      if (ble.discoveredDescriptor(
            result.connectionId, index, candidate,
            TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID) &&
          candidate.descriptorUuid.equalsIgnoreCase(TEST_DESCRIPTOR_UUID))
      {
        descriptor = candidate;
        descriptorFound = true;
      }
    }
    Serial.printf(
      "DATABASE success=%u services_ok=%u chars=%u descs=%u found=%u/%u write_nr=%u context=%s\n",
      result.success ? 1 : 0,
      serviceCount != 0 ? 1 : 0,
      static_cast<unsigned>(characteristicCount),
      static_cast<unsigned>(descriptorCount),
      characteristicFound ? 1 : 0,
      descriptorFound ? 1 : 0,
      characteristicFound && characteristic.writableWithoutResponse ? 1 : 0,
      callbackContext());
    if (result.success && characteristicFound && descriptorFound)
    {
      const bool accepted = ble.discoverCharacteristic(
        result.connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID);
      Serial.println(accepted ? "DISCOVER_REQUESTED" : "DISCOVER_REQUEST_FAILED");
    }
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
      "WRITE phase=%u success=%u response=%u context=%s\n",
      writePhase,
      result.success ? 1 : 0,
      result.response ? 1 : 0,
      callbackContext());
    if (!result.success) return;
    if (writePhase == 0)
    {
      writePhase = 1;
      Serial.println(
        ble.writeCharacteristic(
          result.connectionId,
          TEST_SERVICE_UUID,
          TEST_CHARACTERISTIC_UUID,
          String("central-no-response"),
          false)
          ? "WRITE_NR_REQUESTED"
          : "WRITE_NR_REQUEST_FAILED");
    }
    else
    {
      Serial.println(
        ble.readDescriptor(
          result.connectionId,
          TEST_SERVICE_UUID,
          TEST_CHARACTERISTIC_UUID,
          TEST_DESCRIPTOR_UUID)
          ? "DESCRIPTOR_READ_REQUESTED"
          : "DESCRIPTOR_READ_REQUEST_FAILED");
    }
  });
  ble.onDescriptorRead([](const EspBleGattResult &result) {
    Serial.printf("DESCRIPTOR_READ success=%u value=%s context=%s\n",
      result.success ? 1 : 0, result.value.c_str(), callbackContext());
    if (result.success)
    {
      Serial.println(
        ble.writeDescriptor(
          result.connectionId,
          TEST_SERVICE_UUID,
          TEST_CHARACTERISTIC_UUID,
          TEST_DESCRIPTOR_UUID,
          String("descriptor-written"),
          true)
          ? "DESCRIPTOR_WRITE_REQUESTED"
          : "DESCRIPTOR_WRITE_REQUEST_FAILED");
    }
  });
  ble.onDescriptorWritten([](const EspBleGattResult &result) {
    Serial.printf("DESCRIPTOR_WRITE success=%u response=%u context=%s\n",
      result.success ? 1 : 0, result.response ? 1 : 0, callbackContext());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("DISCONNECTED services=%u context=%s\n",
      static_cast<unsigned>(ble.discoveredServiceCount(connection.id)), callbackContext());
    connectionId = 0;
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
    if (command == 's' && !connectionRequested)
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'x' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_FAILED");
    }
  }

  ble.update();
  delay(1);
}
