// immediate_alert peer_device: EspBle GATT server for the standard Immediate
// Alert Service (the Find Me profile target role). Alert Level (0x2A06) is a
// single Write Without Response uint8 (0 = No Alert, 1 = Mild, 2 = High). The
// server reports each written level from onWritten.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *IMMEDIATE_ALERT_SERVICE_UUID = "1802";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig alertLevelConfig;
  alertLevelConfig.writable = true;              // Write With Response
  alertLevelConfig.writableWithoutResponse = true; // Write Without Response
  auto &server = ble.gattServer();
  if (!server.addService(IMMEDIATE_ALERT_SERVICE_UUID) ||
      !server.addCharacteristic(IMMEDIATE_ALERT_SERVICE_UUID, ALERT_LEVEL_UUID, alertLevelConfig))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID))
      return;
    const uint8_t level = write.value.length() == 1 ? static_cast<uint8_t>(write.value[0]) : 0xFF;
    Serial.printf("ALERT_LEVEL level=%u context=%s\n", level, contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle FindMe Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle FindMe Peer");
  ble.advertising().addServiceUuid(IMMEDIATE_ALERT_SERVICE_UUID);
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
  }
  ble.update();
  delay(1);
}
