// weight_scale DUT: EspBle GATT client for the Weight Scale Service. It reads
// Weight Scale Feature, subscribes to Weight Measurement indications, and
// decodes the uint16 weight (0.005 kg resolution) to grams.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *WEIGHT_SCALE_SERVICE_UUID = "181d";
static constexpr const char *WEIGHT_MEASUREMENT_UUID = "2a9d";
static constexpr const char *WEIGHT_SCALE_FEATURE_UUID = "2a9e";

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
      connection.id, WEIGHT_SCALE_SERVICE_UUID, WEIGHT_SCALE_FEATURE_UUID);
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
        result.connectionId, WEIGHT_SCALE_SERVICE_UUID, WEIGHT_MEASUREMENT_UUID, false);
      Serial.println(accepted ? "WS_SUBSCRIBE_REQUESTED" : "WS_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("WS_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("WS_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(WEIGHT_MEASUREMENT_UUID))
      return;
    const bool valid = notification.value.length() >= 3;
    const uint8_t flags = valid ? static_cast<uint8_t>(notification.value[0]) : 0;
    long grams = 0;
    if (valid)
    {
      const uint16_t raw = static_cast<uint8_t>(notification.value[1]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[2])) << 8);
      grams = static_cast<long>(raw) * 5; // 0.005 kg per LSB = 5 g
    }
    Serial.printf("WS_MEASUREMENT valid=%u indication=%u flags=%02x grams=%ld context=%s\n",
      valid ? 1 : 0, notification.indication ? 1 : 0, flags, grams, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(WEIGHT_SCALE_SERVICE_UUID))
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
        connectionId, WEIGHT_SCALE_SERVICE_UUID, WEIGHT_MEASUREMENT_UUID);
      Serial.println(accepted ? "WS_UNSUBSCRIBE_REQUESTED" : "WS_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
