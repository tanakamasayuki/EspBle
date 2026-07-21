// en: HealthThermometerClient - connect to a Health Thermometer Service
//     (0x1809), read Temperature Type, subscribe to Temperature Measurement
//     indications, and decode the IEEE-11073 32-bit FLOAT temperature.
// ja: HealthThermometerClient - Health Thermometer Service（0x1809）へ接続し、
//     Temperature TypeをRead、Temperature MeasurementのIndicationを購読して、
//     IEEE-11073 32-bit FLOATの温度をデコードする。
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *HEALTH_THERMOMETER_SERVICE_UUID = "1809";
static constexpr const char *TEMPERATURE_MEASUREMENT_UUID = "2a1c";
static constexpr const char *TEMPERATURE_TYPE_UUID = "2a1d";

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
    ble.readCharacteristic(connection.id, HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_TYPE_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 1)
    {
      Serial.printf("Temperature type: %u\n", static_cast<uint8_t>(result.value[0]));
      // en: Temperature Measurement is an indication (notifications = false).
      // ja: Temperature MeasurementはIndication（notifications = false）。
      ble.subscribe(result.connectionId, HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_MEASUREMENT_UUID, false);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(TEMPERATURE_MEASUREMENT_UUID) ||
        notification.value.length() < 5)
      return;
    const uint8_t bytes[4] = {
      static_cast<uint8_t>(notification.value[1]),
      static_cast<uint8_t>(notification.value[2]),
      static_cast<uint8_t>(notification.value[3]),
      static_cast<uint8_t>(notification.value[4])};
    const double temperature = espBleReadMedicalFloat32LE(bytes);
    const uint8_t flags = static_cast<uint8_t>(notification.value[0]);
    Serial.printf("Temperature: %.2f %s\n", temperature, (flags & 0x01) ? "F" : "C");
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(HEALTH_THERMOMETER_SERVICE_UUID))
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
