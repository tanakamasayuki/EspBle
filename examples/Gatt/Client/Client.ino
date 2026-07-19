// en: Client - connect to the Gatt/Server example and run the central GATT client flow:
//     database discovery -> known-UUID discovery -> read -> write -> descriptor access.
//     Each step is a request API;
//     completion arrives as an event from update(). The next operation is chained from
//     the previous one's completion callback (central GATT operations are one-at-a-time).
// ja: Client - Gatt/Server example へ接続し、CentralのGATT Clientフローを一通り実行する:
//     Database一覧Discovery → 既知UUIDのDiscovery → Read → Write → Descriptor操作。
//     各ステップは要求APIで受理され、
//     完了は update() 経由のイベントとして届く。次の操作は前の操作の完了callbackから連鎖する
//     （Central GATT操作は同時1件のため）。
#include <EspBle.h>

// en: Same UUIDs as the Gatt/Server example.
// ja: Gatt/Server example と同じUUID。
static constexpr const char *SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";
static constexpr const char *DESCRIPTOR_UUID = "10da4dd2-8eaa-4c69-9003-676174747277";

EspBle ble;
bool connectionRequested = false;
unsigned writePhase = 0;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle GATT Client";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Once connected, enumerate the peer's GATT database.
  // ja: 接続できたらpeerのGATT databaseを一覧Discoveryする。
  ble.onConnected([](const EspBleConnection &connection) {
    if (!ble.discoverServices(connection.id))
    {
      Serial.printf("Discovery request failed: %s\n", ble.lastErrorDetail().c_str());
    }
  });
  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Database discovery failed: %s\n", result.detail.c_str());
      return;
    }
    Serial.printf("Services: %u, characteristics: %u, descriptors: %u\n",
      static_cast<unsigned>(ble.discoveredServiceCount(result.connectionId)),
      static_cast<unsigned>(ble.discoveredCharacteristicCount(result.connectionId)),
      static_cast<unsigned>(ble.discoveredDescriptorCount(result.connectionId)));
    ble.discoverCharacteristic(result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID);
  });
  // en: Discovery done -> request a read.
  // ja: Discovery完了 → Readを要求。
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Discovery failed: %s\n", result.detail.c_str());
      return;
    }
    ble.readCharacteristic(result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID);
  });
  // en: Read done -> print the value and request a write (5th arg true = write with response).
  // ja: Read完了 → 値を表示し、Writeを要求（第5引数true = Write with Response）。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Read failed: %s\n", result.detail.c_str());
      return;
    }
    Serial.printf("Read: %s\n", result.value.c_str());
    ble.writeCharacteristic(
      result.connectionId,
      SERVICE_UUID,
      CHARACTERISTIC_UUID,
      String("hello from Central"),
      true);
  });
  // en: Follow the acknowledged write with Write Without Response, then read a descriptor.
  // ja: 応答ありWriteの次にWrite Without Responseを行い、Descriptorを読む。
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Write failed: %s\n", result.detail.c_str());
      return;
    }
    if (writePhase++ == 0)
    {
      ble.writeCharacteristic(
        result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID,
        String("unacknowledged Central write"), false);
    }
    else
    {
      ble.readDescriptor(
        result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID, DESCRIPTOR_UUID);
    }
  });
  ble.onDescriptorRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Descriptor read failed: %s\n", result.detail.c_str());
      return;
    }
    Serial.printf("Descriptor: %s\n", result.value.c_str());
    ble.writeDescriptor(
      result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID, DESCRIPTOR_UUID,
      String("updated description"), true);
  });
  ble.onDescriptorWritten([](const EspBleGattResult &result) {
    Serial.println(result.success ? "Descriptor write complete" : "Descriptor write failed");
  });

  // en: Connect once the target service UUID is found.
  // ja: 対象Service UUIDを見つけたら接続する。
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  if (!ble.scanner().start(scanConfig))
  {
    Serial.printf("Scan start failed: %s\n", ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  // en: Discovery/read/write completion events are delivered from this update().
  // ja: Discovery/Read/Write の各完了イベントはこの update() から配送される。
  ble.update();
  delay(1);
}
