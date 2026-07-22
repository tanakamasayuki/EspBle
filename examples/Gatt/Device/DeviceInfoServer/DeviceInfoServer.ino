#include <EspBle.h>

static constexpr const char *DEVICE_INFORMATION_SERVICE_UUID = "180a";
static constexpr const char *MANUFACTURER_NAME_UUID = "2a29";
static constexpr const char *MODEL_NUMBER_UUID = "2a24";
static constexpr const char *FIRMWARE_REVISION_UUID = "2a26";
static constexpr const char *PNP_ID_UUID = "2a50";

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig readable;
  readable.readable = true;
  auto &server = ble.gattServer();
  const uint8_t pnpId[] = {
    0x02,       // USB Implementers Forum vendor ID source
    0x34, 0x12, // Vendor ID 0x1234, little-endian
    0x78, 0x56, // Product ID 0x5678, little-endian
    0x00, 0x01  // Product version 0x0100, little-endian
  };
  if (!server.addService(DEVICE_INFORMATION_SERVICE_UUID) ||
      !server.addCharacteristic(
        DEVICE_INFORMATION_SERVICE_UUID, MANUFACTURER_NAME_UUID, readable) ||
      !server.addCharacteristic(
        DEVICE_INFORMATION_SERVICE_UUID, MODEL_NUMBER_UUID, readable) ||
      !server.addCharacteristic(
        DEVICE_INFORMATION_SERVICE_UUID, FIRMWARE_REVISION_UUID, readable) ||
      !server.addCharacteristic(DEVICE_INFORMATION_SERVICE_UUID, PNP_ID_UUID, readable) ||
      !server.setValue(
        DEVICE_INFORMATION_SERVICE_UUID, MANUFACTURER_NAME_UUID, String("EspBle")) ||
      !server.setValue(
        DEVICE_INFORMATION_SERVICE_UUID, MODEL_NUMBER_UUID, String("DeviceInfoServer")) ||
      !server.setValue(
        DEVICE_INFORMATION_SERVICE_UUID, FIRMWARE_REVISION_UUID, String(ESPBLE_VERSION_STR)) ||
      !server.setValue(
        DEVICE_INFORMATION_SERVICE_UUID, PNP_ID_UUID, pnpId, sizeof(pnpId)))
  {
    Serial.printf("Device Information configuration failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Device Info";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.advertising().setName("EspBle Device Info");
  ble.advertising().addServiceUuid(DEVICE_INFORMATION_SERVICE_UUID);
  ble.advertising().start();
  Serial.println("Device Information Service is ready.");
}

void loop()
{
  ble.update();
  delay(1);
}
