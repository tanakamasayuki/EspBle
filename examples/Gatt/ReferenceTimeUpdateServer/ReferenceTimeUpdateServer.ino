// en: ReferenceTimeUpdateServer - standard Reference Time Update Service
//     (0x1806). Time Update Control Point (0x2A16) is Write Without Response
//     (1 = Get Reference Update, 2 = Cancel Reference Update); Time Update State
//     (0x2A17) is a readable 2-byte value (Current State + Result). Control
//     Point writes transition the read-only state.
// ja: ReferenceTimeUpdateServer - 標準Reference Time Update Service（0x1806）。
//     Time Update Control Point（0x2A16）はWrite Without Response（1 = Get
//     Reference Update、2 = Cancel）、Time Update State（0x2A17）はread可能な
//     2バイト値（Current State＋Result）。Control Point書き込みでread専用stateを
//     遷移させる。
#include <EspBle.h>

static constexpr const char *RTUS_SERVICE_UUID = "1806";
static constexpr const char *TIME_UPDATE_CONTROL_POINT_UUID = "2a16";
static constexpr const char *TIME_UPDATE_STATE_UUID = "2a17";

EspBle ble;
// en: Current State: 0 = Idle, 1 = Update Pending. Result: 0 = Successful, 1 = Canceled.
// ja: Current State: 0 = Idle、1 = Update Pending。Result: 0 = Successful、1 = Canceled。
uint8_t timeUpdateState[2] = {0, 0};

static void setState(uint8_t current, uint8_t resultCode)
{
  timeUpdateState[0] = current;
  timeUpdateState[1] = resultCode;
  ble.gattServer().setValue(RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID, timeUpdateState, sizeof(timeUpdateState));
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writableWithoutResponse = true;
  EspBleGattCharacteristicConfig stateConfig;
  stateConfig.readable = true;
  auto &server = ble.gattServer();
  server.addService(RTUS_SERVICE_UUID);
  server.addCharacteristic(RTUS_SERVICE_UUID, TIME_UPDATE_CONTROL_POINT_UUID, controlConfig);
  server.addCharacteristic(RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID, stateConfig);
  server.setValue(RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID, timeUpdateState, sizeof(timeUpdateState));

  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(TIME_UPDATE_CONTROL_POINT_UUID) || write.value.length() != 1)
      return;
    const uint8_t command = static_cast<uint8_t>(write.value[0]);
    if (command == 1)
    {
      Serial.println("Get Reference Update");
      setState(1, 0); // en: Update Pending / ja: Update Pending
    }
    else if (command == 2)
    {
      Serial.println("Cancel Reference Update");
      setState(0, 1); // en: Idle, Canceled / ja: Idle、Canceled
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Time Update";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(RTUS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  ble.update();
  delay(1);
}
