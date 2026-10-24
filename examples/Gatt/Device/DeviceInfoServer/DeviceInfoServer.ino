// en: Standard Device Information strings and PnP ID
// ja: 標準Device Information文字列とPnP ID
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

  EspBleGattCharacteristicConfig readable;
  readable.readable = true;
  auto &server = ble.gattServer();
  const uint8_t pnpId[] = {
    0x02,       // USB Implementers Forum vendor ID source
    0x34, 0x12, // Vendor ID 0x1234, little-endian
    0x78, 0x56, // Product ID 0x5678, little-endian
    0x00, 0x01  // Product version 0x0100, little-endian
  };
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  if (!(deviceInformationServiceService = server.addService(DEVICE_INFORMATION_SERVICE_UUID)).valid() ||
      !(manufacturerNameCharacteristic = server.addCharacteristic(deviceInformationServiceService, MANUFACTURER_NAME_UUID, readable)).valid() ||
      !(modelNumberCharacteristic = server.addCharacteristic(deviceInformationServiceService, MODEL_NUMBER_UUID, readable)).valid() ||
      !(firmwareRevisionCharacteristic = server.addCharacteristic(deviceInformationServiceService, FIRMWARE_REVISION_UUID, readable)).valid() ||
      !(pnpIdCharacteristic = server.addCharacteristic(deviceInformationServiceService, PNP_ID_UUID, readable)).valid() ||
      !server.setValue(manufacturerNameCharacteristic, String("EspBle")) ||
      !server.setValue(modelNumberCharacteristic, String("DeviceInfoServer")) ||
      !server.setValue(firmwareRevisionCharacteristic, String(ESPBLE_VERSION_STR)) ||
      !server.setValue(pnpIdCharacteristic, pnpId, sizeof(pnpId)))
  {
    Serial.printf("Device Information configuration failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Device Info";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
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
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
