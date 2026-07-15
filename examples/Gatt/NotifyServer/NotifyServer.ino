#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";
static constexpr const char *CHARACTERISTIC_UUID = "71756361-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
bool hasNotificationSubscriber = false;
uint32_t lastNotification = 0;
uint32_t counter = 0;

void setup()
{
  Serial.begin(115200);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig counterConfig;
  counterConfig.readable = true;
  counterConfig.notifiable = true;
  gattServer.addService(SERVICE_UUID);
  gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, counterConfig);
  gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    hasNotificationSubscriber = subscription.notifications;
  });
  gattServer.onSent([](const EspBleGattSendResult &result) {
    if (!result.success)
    {
      Serial.printf("Notification failed: %s\n", result.detail.c_str());
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Notify Server";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Notify Server");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();
}

void loop()
{
  ble.update();

  if (hasNotificationSubscriber && millis() - lastNotification >= 1000)
  {
    lastNotification = millis();
    const String value = String(++counter);
    ble.gattServer().notify(SERVICE_UUID, CHARACTERISTIC_UUID, value);
  }
  delay(1);
}
