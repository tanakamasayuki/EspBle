#include <EspBle.h>

static constexpr const char *CURRENT_TIME_SERVICE_UUID = "1805";
static constexpr const char *CURRENT_TIME_UUID = "2a2b";

EspBle ble;
bool connectionRequested = false;

static void printCurrentTime(const String &value, const char *prefix)
{
  if (value.length() != 10)
  {
    Serial.printf("%s has invalid length: %u\n", prefix,
      static_cast<unsigned>(value.length()));
    return;
  }
  const uint16_t year = static_cast<uint8_t>(value[0]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8);
  Serial.printf("%s: %04u-%02u-%02u %02u:%02u:%02u weekday=%u fraction=%u/256 reason=0x%02x\n",
    prefix, year,
    static_cast<uint8_t>(value[2]), static_cast<uint8_t>(value[3]),
    static_cast<uint8_t>(value[4]), static_cast<uint8_t>(value[5]),
    static_cast<uint8_t>(value[6]), static_cast<uint8_t>(value[7]),
    static_cast<uint8_t>(value[8]), static_cast<uint8_t>(value[9]));
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
    ble.readCharacteristic(connection.id, CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Current Time read failed: %s\n", result.detail.c_str());
      return;
    }
    printCurrentTime(result.value, "Current Time");
    ble.subscribe(result.connectionId, CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID);
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("Current Time subscription: %s\n",
      result.success ? "ready" : "failed");
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(CURRENT_TIME_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(CURRENT_TIME_UUID))
    {
      printCurrentTime(notification.value, "Current Time changed");
    }
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(CURRENT_TIME_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
  });

  EspBleScanConfig scan;
  scan.active = true;
  ble.scanner().start(scan);
}

void loop()
{
  ble.update();
  delay(1);
}
