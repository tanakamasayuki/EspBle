// en: ProximityServer - the Proximity profile Reporter role. It hosts two
//     standard services at once: the Link Loss Service (0x1803) with a
//     read/write Alert Level (0x2A06), and the Tx Power Service (0x1804) with a
//     read-only signed-int8 Tx Power Level (0x2A07). A real Reporter alerts on
//     link loss according to the configured Alert Level.
// ja: ProximityServer - ProximityプロファイルのReporter役。2つの標準Serviceを
//     同時にホストする: Link Loss Service（0x1803、read/writeのAlert Level
//     0x2A06）と、Tx Power Service（0x1804、read専用のsigned int8 Tx Power Level
//     0x2A07）。実機のReporterは設定されたAlert Levelに従いlink loss時に鳴動する。
#include <EspBle.h>

static constexpr const char *LINK_LOSS_SERVICE_UUID = "1803";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";
static constexpr const char *TX_POWER_SERVICE_UUID = "1804";
static constexpr const char *TX_POWER_LEVEL_UUID = "2a07";

EspBle ble;
const int8_t txPowerLevel = -8; // en: dBm / ja: dBm

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig alertLevelConfig;
  alertLevelConfig.readable = true;
  alertLevelConfig.writable = true;
  EspBleGattCharacteristicConfig txPowerConfig;
  txPowerConfig.readable = true;
  auto &server = ble.gattServer();
  const uint8_t initialAlert = 0; // en: No Alert / ja: No Alert
  const uint8_t txPowerByte = static_cast<uint8_t>(txPowerLevel);
  server.addService(LINK_LOSS_SERVICE_UUID);
  server.addCharacteristic(LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, alertLevelConfig);
  server.addService(TX_POWER_SERVICE_UUID);
  server.addCharacteristic(TX_POWER_SERVICE_UUID, TX_POWER_LEVEL_UUID, txPowerConfig);
  server.setValue(LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, &initialAlert, sizeof(initialAlert));
  server.setValue(TX_POWER_SERVICE_UUID, TX_POWER_LEVEL_UUID, &txPowerByte, sizeof(txPowerByte));

  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID) || write.value.length() != 1)
      return;
    const uint8_t level = static_cast<uint8_t>(write.value[0]);
    ble.gattServer().setValue(LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, &level, sizeof(level));
    const char *name = level == 0 ? "No Alert" : (level == 1 ? "Mild Alert" : "High Alert");
    Serial.printf("Link Loss Alert Level: %u (%s)\n", level, name);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Proximity";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(LINK_LOSS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  ble.update();
  delay(1);
}
