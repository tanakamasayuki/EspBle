// en: ReferenceTimeUpdateClient - connect to a Reference Time Update Service
//     (0x1806), read the Time Update State, write the Time Update Control Point
//     (Write Without Response) to request then cancel a reference update, and
//     re-read the state each time.
// ja: ReferenceTimeUpdateClient - Reference Time Update Service（0x1806）へ接続し、
//     Time Update StateをRead、Time Update Control Point（Write Without Response）
//     でreference update要求→キャンセルを行い、毎回stateを再readする。
#include <EspBle.h>

static constexpr const char *RTUS_SERVICE_UUID = "1806";
static constexpr const char *TIME_UPDATE_CONTROL_POINT_UUID = "2a16";
static constexpr const char *TIME_UPDATE_STATE_UUID = "2a17";

EspBle ble;
EspBleConnectionId connectionId = 0;
unsigned long lastAction = 0;
int step = 0;

static void writeControlPoint(uint8_t command)
{
  // en: Write Without Response. / ja: Write Without Response。
  ble.writeCharacteristic(
    connectionId, RTUS_SERVICE_UUID, TIME_UPDATE_CONTROL_POINT_UUID, &command, sizeof(command), false);
}

void setup()
{
  Serial.begin(115200);
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    lastAction = millis();
    step = 0;
    ble.readCharacteristic(connection.id, RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(TIME_UPDATE_STATE_UUID) &&
        result.success && result.value.length() == 2)
    {
      const uint8_t current = static_cast<uint8_t>(result.value[0]);
      const uint8_t resultCode = static_cast<uint8_t>(result.value[1]);
      Serial.printf("Time Update State: current=%u result=%u\n", current, resultCode);
    }
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(RTUS_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  // en: Walk through request -> read -> cancel -> read, two seconds apart.
  // ja: 2秒間隔で 要求 → read → キャンセル → read を進める。
  if (connectionId != 0 && millis() - lastAction >= 2000)
  {
    lastAction = millis();
    if (step == 0)
      writeControlPoint(1); // en: Get Reference Update / ja: Get Reference Update
    else if (step == 1)
      ble.readCharacteristic(connectionId, RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID);
    else if (step == 2)
      writeControlPoint(2); // en: Cancel Reference Update / ja: Cancel Reference Update
    else if (step == 3)
      ble.readCharacteristic(connectionId, RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID);
    if (step <= 3)
      ++step;
  }

  ble.update();
  delay(1);
}
