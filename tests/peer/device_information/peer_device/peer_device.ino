#include <EspBle.h>

static constexpr const char *DEVICE_INFORMATION_SERVICE_UUID = "180a";
static constexpr const char *MANUFACTURER_NAME_UUID = "2a29";
static constexpr const char *MODEL_NUMBER_UUID = "2a24";
static constexpr const char *FIRMWARE_REVISION_UUID = "2a26";
static constexpr const char *PNP_ID_UUID = "2a50";

EspBle ble;

EspBleGattService deviceInformationServiceService;
EspBleGattCharacteristic manufacturerNameCharacteristic;
EspBleGattCharacteristic modelNumberCharacteristic;
EspBleGattCharacteristic firmwareRevisionCharacteristic;
EspBleGattCharacteristic pnpIdCharacteristic;
void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleGattCharacteristicConfig readable;
  readable.readable = true;
  const uint8_t pnpId[] = {0x02, 0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a};
  auto &server = ble.gattServer();
  if (!(deviceInformationServiceService = server.addService(DEVICE_INFORMATION_SERVICE_UUID)).valid() ||
      !(manufacturerNameCharacteristic = server.addCharacteristic(deviceInformationServiceService, MANUFACTURER_NAME_UUID, readable)).valid() ||
      !(modelNumberCharacteristic = server.addCharacteristic(deviceInformationServiceService, MODEL_NUMBER_UUID, readable)).valid() ||
      !(firmwareRevisionCharacteristic = server.addCharacteristic(deviceInformationServiceService, FIRMWARE_REVISION_UUID, readable)).valid() ||
      !(pnpIdCharacteristic = server.addCharacteristic(deviceInformationServiceService, PNP_ID_UUID, readable)).valid() ||
      !server.setValue(manufacturerNameCharacteristic, String("EspBle")) ||
      !server.setValue(modelNumberCharacteristic, String("DIS-Peer")) ||
      !server.setValue(firmwareRevisionCharacteristic, String("1.2.3")) ||
      !server.setValue(pnpIdCharacteristic, pnpId, sizeof(pnpId)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle DIS Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle DIS Peer");
  ble.advertising().addServiceUuid(DEVICE_INFORMATION_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == '?')
  {
    Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
  }
  ble.update();
  delay(1);
}
