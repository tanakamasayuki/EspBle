// body_composition DUT: EspBle GATT client for the Body Composition Service. It
// reads Body Composition Feature, subscribes to Body Composition Measurement
// indications, and decodes the mandatory Body Fat Percentage (0.1 %/LSB) and the
// optional Weight field (0.005 kg/LSB in SI) present per the measurement flags.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *BODY_COMPOSITION_SERVICE_UUID = "181b";
static constexpr const char *BODY_COMPOSITION_MEASUREMENT_UUID = "2a9c";
static constexpr const char *BODY_COMPOSITION_FEATURE_UUID = "2a9b";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();
  if (!ble.begin())
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    const bool accepted = ble.readCharacteristic(
      connection.id, BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_FEATURE_UUID);
    Serial.println(accepted ? "FEATURE_READ_REQUESTED" : "FEATURE_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 4;
    uint32_t value = 0;
    if (valid)
    {
      for (int i = 3; i >= 0; --i)
        value = (value << 8) | static_cast<uint8_t>(result.value[i]);
    }
    Serial.printf("FEATURE_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0, static_cast<unsigned>(value), contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(
        result.connectionId, BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, false);
      Serial.println(accepted ? "BC_SUBSCRIBE_REQUESTED" : "BC_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("BC_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("BC_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(BODY_COMPOSITION_MEASUREMENT_UUID))
      return;
    // Minimum layout: flags(2) + Body Fat Percentage(2). Weight, when present,
    // follows the mandatory field because no other optional field is set here.
    const size_t length = notification.value.length();
    const bool valid = length >= 4;
    uint16_t flags = 0;
    long fatTenths = 0;
    long weightGrams = -1;
    if (valid)
    {
      flags = static_cast<uint8_t>(notification.value[0]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[1])) << 8);
      const uint16_t fatRaw = static_cast<uint8_t>(notification.value[2]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[3])) << 8);
      fatTenths = static_cast<long>(fatRaw); // 0.1 % per LSB
      const bool weightPresent = (flags & 0x0400) != 0; // bit 10 = Weight present
      if (weightPresent && length >= 6)
      {
        const uint16_t weightRaw = static_cast<uint8_t>(notification.value[4]) |
          (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[5])) << 8);
        weightGrams = static_cast<long>(weightRaw) * 5; // 0.005 kg per LSB = 5 g
      }
    }
    Serial.printf(
      "BC_MEASUREMENT valid=%u indication=%u flags=%04x fat_tenths=%ld weight_grams=%ld context=%s\n",
      valid ? 1 : 0, notification.indication ? 1 : 0, flags, fatTenths, weightGrams, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(BODY_COMPOSITION_SERVICE_UUID))
      return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
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
      EspBleScanConfig scan;
      scan.active = true;
      Serial.println(ble.scanner().start(scan) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'u' && connectionId != 0)
    {
      const bool accepted = ble.unsubscribe(
        connectionId, BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID);
      Serial.println(accepted ? "BC_UNSUBSCRIBE_REQUESTED" : "BC_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
