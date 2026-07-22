// fitness_machine peer_device: EspBle GATT server for the standard Fitness
// Machine Service (0x1826). Indoor Bike Data (0x2AD2) is a notification; Fitness
// Machine Feature (0x2ACC) is readable; the Fitness Machine Control Point
// (0x2AD9, write + indicate) drives interactive control and Fitness Machine
// Status (0x2ADA) notifies the resulting change. Because BLE sends are
// single-in-flight, the Control Point response indication and the Status
// notification are sequenced from onSent.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *FTMS_SERVICE_UUID = "1826";
static constexpr const char *INDOOR_BIKE_DATA_UUID = "2ad2";
static constexpr const char *FITNESS_MACHINE_FEATURE_UUID = "2acc";
static constexpr const char *CONTROL_POINT_UUID = "2ad9";
static constexpr const char *STATUS_UUID = "2ada";

EspBle ble;
TaskHandle_t loopTask = nullptr;
// Fitness Machine Features = 0x00000006 (Cadence + Total Distance supported),
// Target Setting Features = 0x00000008 (Power Target Setting supported).
const uint8_t feature[8] = {0x06, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};

// Target power set via the Control Point; a Fitness Machine Status "Target
// Power Changed" notification reporting it is sent on the 'g' command.
volatile int16_t targetPower = 0;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig notifyConfig;
  notifyConfig.notifiable = true;
  EspBleGattCharacteristicConfig readConfig;
  readConfig.readable = true;
  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writable = true;
  controlConfig.indicatable = true;
  auto &server = ble.gattServer();
  if (!server.addService(FTMS_SERVICE_UUID) ||
      !server.addCharacteristic(FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID, notifyConfig) ||
      !server.addCharacteristic(FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID, readConfig) ||
      !server.addCharacteristic(FTMS_SERVICE_UUID, CONTROL_POINT_UUID, controlConfig) ||
      !server.addCharacteristic(FTMS_SERVICE_UUID, STATUS_UUID, notifyConfig) ||
      !server.setValue(FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("FTMS_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(CONTROL_POINT_UUID) || write.value.length() < 1)
      return;
    const uint8_t op = static_cast<uint8_t>(write.value[0]);
    uint8_t result = 0x01; // Success
    if (op == 0x00) // Request Control
    {
      Serial.printf("CP_WRITE op=00 request_control context=%s\n", contextName());
    }
    else if (op == 0x05 && write.value.length() == 3) // Set Target Power (sint16 W)
    {
      targetPower = static_cast<int16_t>(
        static_cast<uint8_t>(write.value[1]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(write.value[2])) << 8));
      Serial.printf("CP_WRITE op=05 set_target_power=%d context=%s\n",
        static_cast<int>(targetPower), contextName());
    }
    else
    {
      result = 0x02; // Op Code Not Supported
      Serial.printf("CP_WRITE op=%02x unsupported context=%s\n", op, contextName());
    }
    // Response: Response Code (0x80), request op code, result code.
    const uint8_t response[3] = {0x80, op, result};
    ble.gattServer().indicate(FTMS_SERVICE_UUID, CONTROL_POINT_UUID, response, sizeof(response));
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("FTMS_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle FTMS Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle FTMS Peer");
  ble.advertising().addServiceUuid(FTMS_SERVICE_UUID);
  ble.advertising().start();
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
    else if (command == 'h')
    {
      // flags 0x0044: bit0 clear (Instantaneous Speed present), bit2
      // (Instantaneous Cadence), bit6 (Instantaneous Power). Speed 30.00 km/h
      // (3000 * 0.01), cadence 90 rpm (180 * 0.5/min), power 250 W.
      const uint16_t flags = 0x0044;
      const uint16_t speed = 3000;
      const uint16_t cadence = 180;
      const int16_t power = 250;
      uint8_t data[8];
      data[0] = static_cast<uint8_t>(flags & 0xFF);
      data[1] = static_cast<uint8_t>((flags >> 8) & 0xFF);
      data[2] = static_cast<uint8_t>(speed & 0xFF);
      data[3] = static_cast<uint8_t>((speed >> 8) & 0xFF);
      data[4] = static_cast<uint8_t>(cadence & 0xFF);
      data[5] = static_cast<uint8_t>((cadence >> 8) & 0xFF);
      data[6] = static_cast<uint8_t>(static_cast<uint16_t>(power) & 0xFF);
      data[7] = static_cast<uint8_t>((static_cast<uint16_t>(power) >> 8) & 0xFF);
      const bool stored = ble.gattServer().setValue(
        FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID, data, sizeof(data));
      const bool notified = ble.gattServer().notify(
        FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID, data, sizeof(data));
      Serial.printf("FTMS_UPDATED stored=%u notified=%u\n", stored ? 1 : 0, notified ? 1 : 0);
    }
    else if (command == 'g')
    {
      // Fitness Machine Status: Target Power Changed (0x08) + sint16 watts,
      // reporting the value set via the Control Point.
      const uint8_t status[3] = {
        0x08,
        static_cast<uint8_t>(static_cast<uint16_t>(targetPower) & 0xFF),
        static_cast<uint8_t>((static_cast<uint16_t>(targetPower) >> 8) & 0xFF)};
      const bool notified = ble.gattServer().notify(FTMS_SERVICE_UUID, STATUS_UUID, status, sizeof(status));
      Serial.printf("STATUS_SENT notified=%u power=%d\n", notified ? 1 : 0, static_cast<int>(targetPower));
    }
  }
  ble.update();
  delay(1);
}
