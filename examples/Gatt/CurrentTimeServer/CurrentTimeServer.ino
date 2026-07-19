#include <EspBle.h>

static constexpr const char *CURRENT_TIME_SERVICE_UUID = "1805";
static constexpr const char *CURRENT_TIME_UUID = "2a2b";

EspBle ble;
// 2026-07-19 12:34:56, Sunday, 0/256 second, manually adjusted.
uint8_t currentTime[] = {0xea, 0x07, 7, 19, 12, 34, 56, 7, 0, 0x01};

static void publishCurrentTime()
{
  auto &server = ble.gattServer();
  server.setValue(CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID,
    currentTime, sizeof(currentTime));
  const bool notified = server.notify(CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID,
    currentTime, sizeof(currentTime));
  Serial.printf("Time: %04u-%02u-%02u %02u:%02u:%02u (notification accepted: %u)\n",
    static_cast<unsigned>(currentTime[0] | (currentTime[1] << 8)),
    currentTime[2], currentTime[3], currentTime[4], currentTime[5], currentTime[6],
    notified ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig timeConfig;
  timeConfig.readable = true;
  timeConfig.notifiable = true;
  auto &server = ble.gattServer();
  if (!server.addService(CURRENT_TIME_SERVICE_UUID) ||
      !server.addCharacteristic(CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID, timeConfig) ||
      !server.setValue(CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID,
        currentTime, sizeof(currentTime)))
  {
    Serial.printf("Current Time configuration failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Current Time";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Current Time");
  ble.advertising().addServiceUuid(CURRENT_TIME_SERVICE_UUID);
  ble.advertising().start();
  Serial.println("Send 't' to advance one second and notify subscribers.");
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 't')
  {
    currentTime[6] = static_cast<uint8_t>((currentTime[6] + 1) % 60);
    publishCurrentTime();
  }
  ble.update();
  delay(1);
}
