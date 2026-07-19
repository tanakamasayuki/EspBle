#include <EspBle.h>

static constexpr const char *DEVICE_INFORMATION_SERVICE_UUID = "180a";
static constexpr const char *CHARACTERISTIC_UUIDS[] = {
  "2a29", // Manufacturer Name
  "2a24", // Model Number
  "2a26", // Firmware Revision
  "2a50"  // PnP ID
};
static constexpr const char *LABELS[] = {
  "Manufacturer", "Model", "Firmware", "PnP ID"
};

EspBle ble;
bool connectionRequested = false;
size_t readIndex = 0;

static bool requestRead(EspBleConnectionId connectionId)
{
  return ble.readCharacteristic(
    connectionId, DEVICE_INFORMATION_SERVICE_UUID, CHARACTERISTIC_UUIDS[readIndex]);
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
    if (!requestRead(connection.id))
    {
      Serial.printf("Read request failed: %s\n", ble.lastErrorDetail().c_str());
    }
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("%s read failed: %s\n", LABELS[readIndex], result.detail.c_str());
      return;
    }

    if (readIndex < 3)
    {
      Serial.printf("%s: %s\n", LABELS[readIndex], result.value.c_str());
    }
    else if (result.value.length() == 7)
    {
      const uint16_t vendorId = static_cast<uint8_t>(result.value[1]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[2])) << 8);
      const uint16_t productId = static_cast<uint8_t>(result.value[3]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[4])) << 8);
      const uint16_t version = static_cast<uint8_t>(result.value[5]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[6])) << 8);
      Serial.printf("PnP ID: source=%u vendor=0x%04x product=0x%04x version=0x%04x\n",
        static_cast<uint8_t>(result.value[0]), vendorId, productId, version);
    }
    else
    {
      Serial.printf("PnP ID has invalid length: %u\n",
        static_cast<unsigned>(result.value.length()));
      return;
    }

    ++readIndex;
    if (readIndex < 4 && !requestRead(result.connectionId))
    {
      Serial.printf("Read request failed: %s\n", ble.lastErrorDetail().c_str());
    }
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested ||
        !result.advertisesService(DEVICE_INFORMATION_SERVICE_UUID)) return;
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
