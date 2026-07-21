// glucose DUT: EspBle GATT client for the Glucose Service RACP procedure. It
// reads Glucose Feature, subscribes to Glucose Measurement (notify) and the
// Record Access Control Point (indicate), writes "Report Stored Records (all)"
// to the RACP, then decodes the notified measurement and the RACP response.
#include <EspBle.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static constexpr const char *GLUCOSE_SERVICE_UUID = "1808";
static constexpr const char *GLUCOSE_MEASUREMENT_UUID = "2a18";
static constexpr const char *GLUCOSE_FEATURE_UUID = "2a51";
static constexpr const char *RACP_UUID = "2a52";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

volatile uint32_t measurementCount = 0;
volatile uint16_t lastSequence = 0;
volatile long lastConcentration = 0;
volatile uint8_t lastTypeLocation = 0;
volatile uint32_t racpResponseCount = 0;
uint8_t racpResponse[8] = {0};
volatile size_t racpResponseLength = 0;

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
    ble.readCharacteristic(connection.id, GLUCOSE_SERVICE_UUID, GLUCOSE_FEATURE_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf("FEATURE_READ valid=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
    if (result.success)
      ble.subscribe(result.connectionId, GLUCOSE_SERVICE_UUID, GLUCOSE_MEASUREMENT_UUID);
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID))
    {
      Serial.printf("MEASUREMENT_SUBSCRIBED success=%u\n", result.success ? 1 : 0);
      if (result.success)
        ble.subscribe(result.connectionId, GLUCOSE_SERVICE_UUID, RACP_UUID, false);
    }
    else if (result.characteristicUuid.equalsIgnoreCase(RACP_UUID))
    {
      Serial.printf("RACP_SUBSCRIBED success=%u\n", result.success ? 1 : 0);
    }
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(RACP_UUID))
      Serial.printf("RACP_WRITTEN success=%u\n", result.success ? 1 : 0);
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID))
    {
      const String &value = notification.value;
      if (value.length() >= 13)
      {
        lastSequence = static_cast<uint8_t>(value[1]) |
          (static_cast<uint16_t>(static_cast<uint8_t>(value[2])) << 8);
        const uint8_t bytes[2] = {static_cast<uint8_t>(value[10]), static_cast<uint8_t>(value[11])};
        lastConcentration = lround(espBleReadMedicalSFloatLE(bytes));
        lastTypeLocation = static_cast<uint8_t>(value[12]);
      }
      ++measurementCount;
    }
    else if (notification.characteristicUuid.equalsIgnoreCase(RACP_UUID))
    {
      const String &value = notification.value;
      racpResponseLength = value.length() < sizeof(racpResponse) ? value.length() : sizeof(racpResponse);
      for (size_t i = 0; i < racpResponseLength; ++i)
        racpResponse[i] = static_cast<uint8_t>(value[i]);
      ++racpResponseCount;
    }
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(GLUCOSE_SERVICE_UUID))
      return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
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
    else if (command == 'r' && connectionId != 0)
    {
      // Report Stored Records, operator All records.
      const uint8_t request[2] = {0x01, 0x01};
      const bool accepted = ble.writeCharacteristic(
        connectionId, GLUCOSE_SERVICE_UUID, RACP_UUID, request, sizeof(request), true);
      Serial.println(accepted ? "RACP_REQUESTED" : "RACP_REQUEST_FAILED");
    }
    else if (command == 'q')
    {
      Serial.printf(
        "GLUCOSE measurements=%u seq=%u concentration=%ld type_location=%02x "
        "racp_responses=%u racp0=%u racp2=%u racp3=%u context=%s\n",
        static_cast<unsigned>(measurementCount), lastSequence, lastConcentration, lastTypeLocation,
        static_cast<unsigned>(racpResponseCount),
        racpResponseLength > 0 ? racpResponse[0] : 0,
        racpResponseLength > 2 ? racpResponse[2] : 0,
        racpResponseLength > 3 ? racpResponse[3] : 0,
        contextName());
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
