#include <EspBle.h>

static constexpr const char *BATTERY_SERVICE_UUID = "180f";
static constexpr const char *BATTERY_LEVEL_UUID = "2a19";

EspBle ble;
uint8_t batteryLevel = 75;

static void publishBatteryLevel()
{
  auto &server = ble.gattServer();
  server.setValue(BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID, &batteryLevel, 1);
  const bool notified = server.notify(
    BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID, &batteryLevel, 1);
  Serial.printf("Battery: %u%% (notification accepted: %u)\n",
    batteryLevel, notified ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig levelConfig;
  levelConfig.readable = true;
  levelConfig.notifiable = true;

  auto &server = ble.gattServer();
  if (!server.addService(BATTERY_SERVICE_UUID) ||
      !server.addCharacteristic(BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID, levelConfig) ||
      !server.setValue(BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID, &batteryLevel, 1))
  {
    Serial.printf("Battery configuration failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("Battery notifications: %u\n", subscription.notifications ? 1 : 0);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Battery";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.advertising().setName("EspBle Battery");
  ble.advertising().addServiceUuid(BATTERY_SERVICE_UUID);
  ble.advertising().start();
  Serial.println("Send '+' or '-' to change the Battery Level.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '+' && batteryLevel < 100)
    {
      ++batteryLevel;
      publishBatteryLevel();
    }
    else if (command == '-' && batteryLevel > 0)
    {
      --batteryLevel;
      publishBatteryLevel();
    }
  }
  ble.update();
  delay(1);
}
