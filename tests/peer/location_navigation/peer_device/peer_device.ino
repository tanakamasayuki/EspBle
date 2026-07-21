// location_navigation peer_device: EspBle GATT server for the standard Location
// and Navigation Service. Location and Speed (0x2A67) is a notification carrying
// uint16 flags plus flags-selected fields; LN Feature (0x2A6A) is a readable
// uint32.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *LN_SERVICE_UUID = "1819";
static constexpr const char *LOCATION_AND_SPEED_UUID = "2a67";
static constexpr const char *LN_FEATURE_UUID = "2a6a";

EspBle ble;
TaskHandle_t loopTask = nullptr;
// bit 0 = Instantaneous Speed Supported, bit 2 = Location Supported.
const uint8_t feature[4] = {0x05, 0x00, 0x00, 0x00};

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
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(LN_SERVICE_UUID) ||
      !server.addCharacteristic(LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID, measurementConfig) ||
      !server.addCharacteristic(LN_SERVICE_UUID, LN_FEATURE_UUID, featureConfig) ||
      !server.setValue(LN_SERVICE_UUID, LN_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("LNS_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("LNS_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle LocNav Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle LocNav Peer");
  ble.advertising().addServiceUuid(LN_SERVICE_UUID);
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
    else if (command == 'n')
    {
      // flags = 0x0005 (Instantaneous Speed + Location present). Speed 5.00 m/s
      // at 1/100 m/s is raw 500. Location is Tokyo: latitude 35.6812 deg and
      // longitude 139.7671 deg at 1e-7 deg/LSB are raw 356812000 and
      // 1397671000. Speed is uint16, lat/lon are sint32, all little-endian.
      const uint16_t flags = 0x0005;
      const uint16_t speed = 500;
      const int32_t latitude = 356812000;
      const int32_t longitude = 1397671000;
      uint8_t measurement[12];
      measurement[0] = static_cast<uint8_t>(flags & 0xFF);
      measurement[1] = static_cast<uint8_t>((flags >> 8) & 0xFF);
      measurement[2] = static_cast<uint8_t>(speed & 0xFF);
      measurement[3] = static_cast<uint8_t>((speed >> 8) & 0xFF);
      const uint32_t lat = static_cast<uint32_t>(latitude);
      const uint32_t lon = static_cast<uint32_t>(longitude);
      measurement[4] = static_cast<uint8_t>(lat & 0xFF);
      measurement[5] = static_cast<uint8_t>((lat >> 8) & 0xFF);
      measurement[6] = static_cast<uint8_t>((lat >> 16) & 0xFF);
      measurement[7] = static_cast<uint8_t>((lat >> 24) & 0xFF);
      measurement[8] = static_cast<uint8_t>(lon & 0xFF);
      measurement[9] = static_cast<uint8_t>((lon >> 8) & 0xFF);
      measurement[10] = static_cast<uint8_t>((lon >> 16) & 0xFF);
      measurement[11] = static_cast<uint8_t>((lon >> 24) & 0xFF);
      const bool stored = ble.gattServer().setValue(
        LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID, measurement, sizeof(measurement));
      const bool notified = ble.gattServer().notify(
        LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID, measurement, sizeof(measurement));
      Serial.printf("LNS_UPDATED stored=%u notified=%u\n", stored ? 1 : 0, notified ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
