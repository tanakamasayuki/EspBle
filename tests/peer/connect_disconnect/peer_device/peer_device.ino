#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-636f6e6e6563";

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *roleName(EspBleRole role)
{
  return role == EspBleRole::Central ? "CENTRAL" : "PERIPHERAL";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleConfig config;
  config.deviceName = "EspBle Peer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf(
      "PERIPHERAL_CONNECTED id=%u role=%s count=%u context=%s\n",
      static_cast<unsigned>(connection.id),
      roleName(connection.localRole),
      static_cast<unsigned>(ble.connectionCount()),
      xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack");
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf(
      "PERIPHERAL_DISCONNECTED id=%u role=%s count=%u context=%s\n",
      static_cast<unsigned>(connection.id),
      roleName(connection.localRole),
      static_cast<unsigned>(ble.connectionCount()),
      xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack");
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == '?')
  {
    Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
  }

  ble.update();
  delay(1);
}
