// en: BloodPressureClient - connect to a Blood Pressure Service (0x1810), read
//     Blood Pressure Feature, subscribe to Blood Pressure Measurement
//     indications, and decode systolic/diastolic/mean IEEE-11073 SFLOATs.
// ja: BloodPressureClient - Blood Pressure Service（0x1810）へ接続し、Blood
//     Pressure FeatureをRead、Blood Pressure MeasurementのIndicationを購読して、
//     systolic/diastolic/meanのIEEE-11073 SFLOATをデコードする。
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *BLOOD_PRESSURE_SERVICE_UUID = "1810";
static constexpr const char *BLOOD_PRESSURE_MEASUREMENT_UUID = "2a35";
static constexpr const char *BLOOD_PRESSURE_FEATURE_UUID = "2a49";

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
    ble.readCharacteristic(connection.id, BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_FEATURE_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 2)
    {
      // en: Blood Pressure Measurement is an indication (notifications = false).
      // ja: Blood Pressure MeasurementはIndication（notifications = false）。
      ble.subscribe(result.connectionId, BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, false);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(BLOOD_PRESSURE_MEASUREMENT_UUID) ||
        notification.value.length() < 7)
      return;
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(notification.value.c_str());
    const double systolic = espBleReadMedicalSFloatLE(&bytes[1]);
    const double diastolic = espBleReadMedicalSFloatLE(&bytes[3]);
    const double mean = espBleReadMedicalSFloatLE(&bytes[5]);
    Serial.printf("Blood pressure: %.0f/%.0f mmHg (mean %.0f)\n", systolic, diastolic, mean);
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(BLOOD_PRESSURE_SERVICE_UUID))
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
