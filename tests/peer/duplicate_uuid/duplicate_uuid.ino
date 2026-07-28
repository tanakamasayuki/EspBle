// Central for the duplicate_uuid peer test: discover the peer's whole database,
// then read and subscribe to each characteristic that shares a UUID by its
// attribute handle.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-647570736572";
static constexpr const char *VALUE_UUID = "5266f728-49d7-4eaf-a6f1-647570636861";
static constexpr size_t MaxTargets = 8;

EspBle ble;
EspBleConnectionId connectionId = 0;
uint16_t targets[MaxTargets];
size_t targetCount = 0;
size_t readIndex = 0;
size_t subscribeIndex = 0;

static void subscribeNext()
{
  if (subscribeIndex >= targetCount)
  {
    Serial.println("SUBSCRIBE_DONE");
    return;
  }
  ble.subscribe(connectionId, targets[subscribeIndex]);
}

static void readNext()
{
  if (readIndex >= targetCount)
  {
    Serial.println("READ_DONE");
    subscribeIndex = 0;
    subscribeNext();
    return;
  }
  ble.readCharacteristic(connectionId, targets[readIndex]);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Duplicate UUID Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionId != 0 || !scanResult.advertisesService(SERVICE_UUID)) return;
    ble.scanner().stop();
    ble.connect(scanResult);
  });

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("CENTRAL_CONNECTED id=%u\n", connection.id);
    ble.discoverServices(connectionId);
  });

  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("DISCOVER_FAILED %s\n", result.detail.c_str());
      return;
    }
    // Count how many services carry the shared UUID.
    size_t services = 0;
    for (size_t i = 0; i < ble.discoveredServiceCount(connectionId); ++i)
    {
      EspBleGattServiceInfo info;
      if (ble.discoveredService(connectionId, i, info) &&
          info.serviceUuid.equalsIgnoreCase(SERVICE_UUID))
      {
        ++services;
      }
    }
    // Collect every characteristic with the shared UUID; only the attribute
    // handle tells them apart.
    targetCount = 0;
    for (size_t i = 0; i < ble.discoveredCharacteristicCount(connectionId); ++i)
    {
      EspBleGattCharacteristicInfo info;
      if (!ble.discoveredCharacteristic(connectionId, i, info)) continue;
      if (!info.characteristicUuid.equalsIgnoreCase(VALUE_UUID)) continue;
      if (targetCount < MaxTargets) targets[targetCount++] = info.handle;
    }
    // Dump the whole database so the attribute table on air is visible.
    for (size_t i = 0; i < ble.discoveredServiceCount(connectionId); ++i)
    {
      EspBleGattServiceInfo info;
      if (ble.discoveredService(connectionId, i, info))
        Serial.printf("SVC handle=%u uuid=%s\n", info.handle, info.serviceUuid.c_str());
    }
    for (size_t i = 0; i < ble.discoveredCharacteristicCount(connectionId); ++i)
    {
      EspBleGattCharacteristicInfo info;
      if (ble.discoveredCharacteristic(connectionId, i, info))
        Serial.printf("CHR handle=%u svc=%s uuid=%s\n",
          info.handle, info.serviceUuid.c_str(), info.characteristicUuid.c_str());
    }
    Serial.printf(
      "DISCOVERED services=%u characteristics=%u\n",
      static_cast<unsigned>(services),
      static_cast<unsigned>(targetCount));
    readIndex = 0;
    readNext();
  });

  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf(
      "READ handle=%u value=%s\n", result.handle, result.value.c_str());
    ++readIndex;
    readNext();
  });

  // The second characteristic's CCCD is written over raw ATT too, and its
  // notifications arrive through the GAP event listener rather than the
  // wrapper's callback.
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf(
      "SUBSCRIBED handle=%u ok=%u\n", result.handle, result.success ? 1 : 0);
    ++subscribeIndex;
    subscribeNext();
  });

  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf(
      "NOTIFY handle=%u value=%s\n", notification.handle, notification.value.c_str());
  });
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 'c')
  {
    EspBleScanConfig scanConfig;
    scanConfig.active = true;
    Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
  }
  ble.update();
  delay(1);
}
