// proximity peer_device: EspBle GATT server for the Proximity profile. It hosts
// two standard services at once: the Link Loss Service (0x1803) with a
// read/write Alert Level (0x2A06), and the Tx Power Service (0x1804) with a
// read-only signed-int8 Tx Power Level (0x2A07). The Alert Level write is
// received in onWritten and stored so the client can read it back.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *LINK_LOSS_SERVICE_UUID = "1803";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";
static constexpr const char *TX_POWER_SERVICE_UUID = "1804";
static constexpr const char *TX_POWER_LEVEL_UUID = "2a07";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const int8_t txPowerLevel = -8; // dBm

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
  alertLevelConfig.readable = true;
  alertLevelConfig.writable = true;
  EspBleGattCharacteristicConfig txPowerConfig;
  txPowerConfig.readable = true;
  auto &server = ble.gattServer();
  const uint8_t initialAlert = 0; // No Alert
  const uint8_t txPowerByte = static_cast<uint8_t>(txPowerLevel);
  if (!server.addService(LINK_LOSS_SERVICE_UUID) ||
      !server.addCharacteristic(LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, alertLevelConfig) ||
      !server.addService(TX_POWER_SERVICE_UUID) ||
      !server.addCharacteristic(TX_POWER_SERVICE_UUID, TX_POWER_LEVEL_UUID, txPowerConfig) ||
      !server.setValue(LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, &initialAlert, sizeof(initialAlert)) ||
      !server.setValue(TX_POWER_SERVICE_UUID, TX_POWER_LEVEL_UUID, &txPowerByte, sizeof(txPowerByte)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID) || write.value.length() != 1)
      return;
    const uint8_t level = static_cast<uint8_t>(write.value[0]);
    ble.gattServer().setValue(LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, &level, sizeof(level));
    Serial.printf("ALERT_WRITE level=%u context=%s\n", level, contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Proximity Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Proximity Peer");
  ble.advertising().addServiceUuid(LINK_LOSS_SERVICE_UUID);
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
