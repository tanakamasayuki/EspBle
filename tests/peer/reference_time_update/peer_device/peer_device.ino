// reference_time_update peer_device: EspBle GATT server for the standard
// Reference Time Update Service. Time Update Control Point (0x2A16) is Write
// Without Response (1 = Get Reference Update, 2 = Cancel Reference Update); Time
// Update State (0x2A17) is a readable 2-byte value (Current State + Result).
// Control Point writes transition the read-only state, verified by re-reading.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *RTUS_SERVICE_UUID = "1806";
static constexpr const char *TIME_UPDATE_CONTROL_POINT_UUID = "2a16";
static constexpr const char *TIME_UPDATE_STATE_UUID = "2a17";

EspBle ble;
TaskHandle_t loopTask = nullptr;
// Current State: 0 = Idle, 1 = Update Pending. Result: 0 = Successful, 1 = Canceled.
uint8_t timeUpdateState[2] = {0, 0};

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static void setState(uint8_t current, uint8_t resultCode)
{
  timeUpdateState[0] = current;
  timeUpdateState[1] = resultCode;
  ble.gattServer().setValue(RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID, timeUpdateState, sizeof(timeUpdateState));
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writableWithoutResponse = true;
  EspBleGattCharacteristicConfig stateConfig;
  stateConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(RTUS_SERVICE_UUID) ||
      !server.addCharacteristic(RTUS_SERVICE_UUID, TIME_UPDATE_CONTROL_POINT_UUID, controlConfig) ||
      !server.addCharacteristic(RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID, stateConfig) ||
      !server.setValue(RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID, timeUpdateState, sizeof(timeUpdateState)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(TIME_UPDATE_CONTROL_POINT_UUID) || write.value.length() != 1)
      return;
    const uint8_t command = static_cast<uint8_t>(write.value[0]);
    Serial.printf("CONTROL_WRITE command=%u context=%s\n", command, contextName());
    // 1 = Get Reference Update, 2 = Cancel Reference Update.
    if (command == 1)
      setState(1, 0); // Update Pending, Successful
    else if (command == 2)
      setState(0, 1); // Idle, Canceled
  });

  EspBleConfig config;
  config.deviceName = "EspBle RTUS Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle RTUS Peer");
  ble.advertising().addServiceUuid(RTUS_SERVICE_UUID);
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
