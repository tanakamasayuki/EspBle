// bond_management peer_device: EspBle GATT server for the standard Bond
// Management Service. Bond Management Feature (0x2AA5) is a readable uint24 bit
// field of supported operations; the Bond Management Control Point (0x2AA4) is
// writable and receives op codes in onWritten. This test validates the GATT
// choreography (feature read + control-point op code), not actual bond deletion,
// so the server only reports the received op code and keeps the link up.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *BMS_SERVICE_UUID = "181e";
static constexpr const char *BOND_MANAGEMENT_CONTROL_POINT_UUID = "2aa4";
static constexpr const char *BOND_MANAGEMENT_FEATURE_UUID = "2aa5";

EspBle ble;
TaskHandle_t loopTask = nullptr;
// bit 0 = Delete bond of requesting device (LE) supported,
// bit 4 = Delete all bonds of requesting device (LE and BR/EDR) supported.
const uint8_t feature[3] = {0x11, 0x00, 0x00};

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(BMS_SERVICE_UUID) ||
      !server.addCharacteristic(BMS_SERVICE_UUID, BOND_MANAGEMENT_CONTROL_POINT_UUID, controlConfig) ||
      !server.addCharacteristic(BMS_SERVICE_UUID, BOND_MANAGEMENT_FEATURE_UUID, featureConfig) ||
      !server.setValue(BMS_SERVICE_UUID, BOND_MANAGEMENT_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(BOND_MANAGEMENT_CONTROL_POINT_UUID) || write.value.length() < 1)
      return;
    const uint8_t opCode = static_cast<uint8_t>(write.value[0]);
    Serial.printf("CONTROL_WRITE opcode=%u length=%u context=%s\n",
      opCode, static_cast<unsigned>(write.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Bond Mgmt Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Bond Mgmt Peer");
  ble.advertising().addServiceUuid(BMS_SERVICE_UUID);
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
