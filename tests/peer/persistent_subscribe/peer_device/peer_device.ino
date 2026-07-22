#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "70657273-7375-6273-9003-7375627363cc";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "70657273-7375-6273-9003-7375627363cd";

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.notifiable = true;
  if (!gattServer.addService(TEST_SERVICE_UUID) ||
      !gattServer.addCharacteristic(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, characteristicConfig))
  {
    Serial.printf("GATT_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf(
      "SUBSCRIPTION id=%u notifications=%u context=%s\n",
      static_cast<unsigned>(subscription.connectionId),
      subscription.notifications ? 1 : 0,
      callbackContext());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Persistent Peer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  // Re-advertise after a disconnect so the central can reconnect for the
  // persistent-subscription restore.
  ble.onDisconnected([](const EspBleConnection &) {
    ble.advertising().start();
    Serial.println("READVERTISING");
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Persistent Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
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
      Serial.println(
        ble.gattServer().notify(
          TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("notify-value"))
          ? "NOTIFY_REQUESTED"
          : "NOTIFY_REQUEST_FAILED");
    }
  }

  ble.update();
  delay(1);
}
