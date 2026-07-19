#include <EspBle.h>

static constexpr const char *ENVIRONMENTAL_SENSING_SERVICE_UUID = "181a";
static constexpr const char *CHARACTERISTIC_UUIDS[] = {"2a6e", "2a6f", "2a6d"};
static constexpr const char *TEMPERATURE_UUID = "2a6e";

EspBle ble;
bool connectionRequested = false;
size_t readIndex = 0;

static uint16_t decode16(const String &value)
{
  return static_cast<uint8_t>(value[0]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8);
}

static uint32_t decode32(const String &value)
{
  return static_cast<uint8_t>(value[0]) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[1])) << 8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[2])) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[3])) << 24);
}

static void printTemperature(const String &value, const char *label)
{
  if (value.length() != 2)
  {
    Serial.printf("%s has invalid length\n", label);
    return;
  }
  Serial.printf("%s raw: %d (0.01 C units)\n",
    label, static_cast<int16_t>(decode16(value)));
}

static bool requestRead(EspBleConnectionId connectionId)
{
  return ble.readCharacteristic(connectionId, ENVIRONMENTAL_SENSING_SERVICE_UUID,
    CHARACTERISTIC_UUIDS[readIndex]);
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
    readIndex = 0;
    requestRead(connection.id);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Environmental read failed: %s\n", result.detail.c_str());
      return;
    }
    if (readIndex == 0)
    {
      printTemperature(result.value, "Temperature");
    }
    else if (readIndex == 1 && result.value.length() == 2)
    {
      Serial.printf("Humidity raw: %u (0.01 %% units)\n", decode16(result.value));
    }
    else if (readIndex == 2 && result.value.length() == 4)
    {
      Serial.printf("Pressure raw: %lu (0.1 Pa units)\n",
        static_cast<unsigned long>(decode32(result.value)));
    }
    else
    {
      Serial.println("Environmental value has invalid length");
      return;
    }

    ++readIndex;
    if (readIndex < 3)
    {
      requestRead(result.connectionId);
    }
    else
    {
      ble.subscribe(result.connectionId, ENVIRONMENTAL_SENSING_SERVICE_UUID,
        TEMPERATURE_UUID);
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("Temperature subscription: %s\n", result.success ? "ready" : "failed");
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(ENVIRONMENTAL_SENSING_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(TEMPERATURE_UUID))
    {
      printTemperature(notification.value, "Temperature changed");
    }
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested ||
        !result.advertisesService(ENVIRONMENTAL_SENSING_SERVICE_UUID)) return;
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
