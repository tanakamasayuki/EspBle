// pulse_oximeter DUT: EspBle GATT client for the Pulse Oximeter Service. It
// reads PLX Features, subscribes to PLX Spot-Check Measurement indications, and
// decodes the SpO2 and pulse rate SFLOATs.
#include <EspBle.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static constexpr const char *PLX_SERVICE_UUID = "1822";
static constexpr const char *PLX_SPOT_CHECK_UUID = "2a5e";
static constexpr const char *PLX_FEATURES_UUID = "2a60";

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
    const bool accepted = ble.readCharacteristic(connection.id, PLX_SERVICE_UUID, PLX_FEATURES_UUID);
    Serial.println(accepted ? "FEATURES_READ_REQUESTED" : "FEATURES_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() >= 2;
    const uint16_t value = valid
      ? static_cast<uint16_t>(static_cast<uint8_t>(result.value[0]) |
          (static_cast<uint16_t>(static_cast<uint8_t>(result.value[1])) << 8))
      : 0;
    Serial.printf("FEATURES_READ valid=%u value=%u context=%s\n", valid ? 1 : 0, value, contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(result.connectionId, PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, false);
      Serial.println(accepted ? "PLX_SUBSCRIBE_REQUESTED" : "PLX_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("PLX_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("PLX_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(PLX_SPOT_CHECK_UUID))
      return;
    const String &value = notification.value;
    const bool valid = value.length() >= 5;
    const uint8_t flags = valid ? static_cast<uint8_t>(value[0]) : 0;
    long spo2 = 0, pulseRate = 0;
    if (valid)
    {
      const uint8_t spo2Bytes[2] = {static_cast<uint8_t>(value[1]), static_cast<uint8_t>(value[2])};
      const uint8_t prBytes[2] = {static_cast<uint8_t>(value[3]), static_cast<uint8_t>(value[4])};
      spo2 = lround(espBleReadMedicalSFloatLE(spo2Bytes));
      pulseRate = lround(espBleReadMedicalSFloatLE(prBytes));
    }
    Serial.printf("PLX_MEASUREMENT valid=%u indication=%u flags=%02x spo2=%ld pulse=%ld context=%s\n",
      valid ? 1 : 0, notification.indication ? 1 : 0, flags, spo2, pulseRate, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(PLX_SERVICE_UUID))
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
      const bool accepted = ble.unsubscribe(connectionId, PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID);
      Serial.println(accepted ? "PLX_UNSUBSCRIBE_REQUESTED" : "PLX_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
