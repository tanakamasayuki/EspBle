// fitness_machine peer_device: EspBle GATT server for the standard Fitness
// Machine Service (0x1826). Indoor Bike Data (0x2AD2) is a notification whose
// layout starts with 16-bit flags followed by the flagged fields; Fitness
// Machine Feature (0x2ACC) is a readable 8-byte pair of feature bitmaps.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *FTMS_SERVICE_UUID = "1826";
static constexpr const char *INDOOR_BIKE_DATA_UUID = "2ad2";
static constexpr const char *FITNESS_MACHINE_FEATURE_UUID = "2acc";

EspBle ble;
TaskHandle_t loopTask = nullptr;
// Fitness Machine Features = 0x00000006 (Cadence + Total Distance supported),
// Target Setting Features = 0x00000000.
const uint8_t feature[8] = {0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig readConfig;
  readConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(FTMS_SERVICE_UUID) ||
      !server.addCharacteristic(FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID, measurementConfig) ||
      !server.addCharacteristic(FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID, readConfig) ||
      !server.setValue(FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("FTMS_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
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
  }
  ble.update();
  delay(1);
}
