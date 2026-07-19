// en: Server - expose a custom GATT service with one readable/writable characteristic.
//     All GATT server configuration must happen before begin() (the backend cannot add
//     services after the server starts). Operate it from Gatt/Client or a generic GATT app.
// ja: Server - Read/Write可能なCharacteristicを1つ持つ独自GATT Serviceを公開する。
//     GATT Server構成はすべて begin() 前に登録する（backendはserver開始後の
//     Service追加ができないため）。Gatt/Client example や汎用GATTアプリから操作できる。
#include <EspBle.h>

// en: Custom 128-bit UUIDs (service / characteristic / descriptor).
// ja: 独自の128-bit UUID（Service / Characteristic / Descriptor）。
static constexpr const char *SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";
static constexpr const char *DESCRIPTOR_UUID = "10da4dd2-8eaa-4c69-9003-676174747277";

EspBle ble;

void setup()
{
  Serial.begin(115200);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;  // en: allow read / ja: Read許可
  valueConfig.writable = true;  // en: allow write / ja: Write許可
  valueConfig.writableWithoutResponse = true;
  EspBleGattDescriptorConfig descriptorConfig;
  descriptorConfig.writable = true;

  // en: Register service -> characteristic -> initial value, all before begin().
  // ja: begin() 前に Service → Characteristic → 初期値 の順で登録する。
  if (!gattServer.addService(SERVICE_UUID) ||
      !gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, valueConfig) ||
      !gattServer.addDescriptor(
        SERVICE_UUID, CHARACTERISTIC_UUID, DESCRIPTOR_UUID, descriptorConfig) ||
      !gattServer.setValue(SERVICE_UUID, CHARACTERISTIC_UUID, String("ready")) ||
      !gattServer.setDescriptorValue(
        SERVICE_UUID, CHARACTERISTIC_UUID, DESCRIPTOR_UUID, String("EspBle value")))
  {
    Serial.printf("GATT configuration failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Write event from a client (a value type with connectionId, UUID, value).
  // ja: Clientからの書込みイベント（connectionId・UUID・値を持つ値型）。
  gattServer.onWritten([](const EspBleGattWrite &write) {
    Serial.printf(
      "Connection %u wrote: %s\n",
      static_cast<unsigned>(write.connectionId),
      write.value.c_str());
  });
  gattServer.onDescriptorWritten([](const EspBleGattWrite &write) {
    Serial.printf(
      "Descriptor %s wrote: %s (connection identified: %u)\n",
      write.descriptorUuid.c_str(),
      write.value.c_str(),
      write.connectionIdentified ? 1 : 0);
  });

  EspBleConfig config;
  config.deviceName = "EspBle GATT Server";
  if (!ble.begin(config)) // en: GATT database is finalized and started here / ja: ここでGATT databaseが確定・開始する
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Advertise the service UUID so clients can find it.
  // ja: ClientがみつけられるようにService UUIDをadvertiseする。
  auto &advertising = ble.advertising();
  advertising.setName("EspBle GATT Server");
  advertising.addServiceUuid(SERVICE_UUID);
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s\n", ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  // en: Write callbacks are delivered from this update().
  // ja: Writeコールバックはこの update() から配送される。
  ble.update();
  delay(1);
}
