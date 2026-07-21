// en: PulseOximeterClient - connect to a Pulse Oximeter Service (0x1822), read
//     PLX Features, subscribe to PLX Spot-Check Measurement indications, and
//     decode the SpO2 and pulse rate SFLOATs.
// ja: PulseOximeterClient - Pulse Oximeter Service（0x1822）へ接続し、PLX
//     FeaturesをRead、PLX Spot-Check MeasurementのIndicationを購読して、SpO2と
//     pulse rateのSFLOATをデコードする。
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *PLX_SERVICE_UUID = "1822";
static constexpr const char *PLX_SPOT_CHECK_UUID = "2a5e";
static constexpr const char *PLX_FEATURES_UUID = "2a60";

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
    ble.readCharacteristic(connection.id, PLX_SERVICE_UUID, PLX_FEATURES_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() >= 2)
    {
      // PLX Spot-Check Measurement is an indication (notifications = false).
      ble.subscribe(result.connectionId, PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, false);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(PLX_SPOT_CHECK_UUID) ||
        notification.value.length() < 5)
      return;
    const String &value = notification.value;
    const uint8_t spo2Bytes[2] = {static_cast<uint8_t>(value[1]), static_cast<uint8_t>(value[2])};
    const uint8_t prBytes[2] = {static_cast<uint8_t>(value[3]), static_cast<uint8_t>(value[4])};
    Serial.printf("SpO2: %.0f %%, pulse: %.0f bpm\n",
      espBleReadMedicalSFloatLE(spo2Bytes), espBleReadMedicalSFloatLE(prBytes));
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(PLX_SERVICE_UUID))
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
