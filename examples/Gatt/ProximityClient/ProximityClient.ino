// en: ProximityClient - the Proximity profile Monitor role. Connect to a
//     Proximity Reporter, read its Tx Power Level (0x2A07, signed int8), read
//     the Link Loss Alert Level (0x2A06), and write a Link Loss Alert Level so
//     the Reporter alerts if the link is later lost.
// ja: ProximityClient - ProximityプロファイルのMonitor役。Proximity Reporterへ
//     接続し、Tx Power Level（0x2A07、signed int8）をRead、Link Loss Alert Level
//     （0x2A06）をRead、そしてLink Loss Alert Levelを書き込んで、後でリンクが
//     切れたときReporterが鳴動するようにする。
#include <EspBle.h>

static constexpr const char *LINK_LOSS_SERVICE_UUID = "1803";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";
static constexpr const char *TX_POWER_SERVICE_UUID = "1804";
static constexpr const char *TX_POWER_LEVEL_UUID = "2a07";

EspBle ble;

void setup()
{
  Serial.begin(115200);
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    ble.readCharacteristic(connection.id, TX_POWER_SERVICE_UUID, TX_POWER_LEVEL_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(TX_POWER_LEVEL_UUID) &&
        result.success && result.value.length() == 1)
    {
      Serial.printf("Tx Power Level: %d dBm\n", static_cast<int8_t>(result.value[0]));
      ble.readCharacteristic(result.connectionId, LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID);
    }
    else if (result.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID) &&
             result.success && result.value.length() == 1)
    {
      Serial.printf("Link Loss Alert Level: %u\n", static_cast<uint8_t>(result.value[0]));
      // en: Arm High Alert (2) so the Reporter alerts on link loss.
      // ja: High Alert（2）を設定し、link loss時にReporterが鳴動するようにする。
      const uint8_t level = 2;
      ble.writeCharacteristic(result.connectionId, LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, &level, sizeof(level), true);
    }
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID) && result.success)
      Serial.println("Armed Link Loss High Alert");
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(LINK_LOSS_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  ble.update();
  delay(1);
}
