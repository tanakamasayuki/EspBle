// en: BodyCompositionClient - connect to a Body Composition Service (0x181B),
//     read Body Composition Feature, subscribe to Body Composition Measurement
//     indications, and decode Body Fat Percentage plus the optional Weight.
// ja: BodyCompositionClient - Body Composition Service（0x181B）へ接続し、Body
//     Composition FeatureをRead、Body Composition MeasurementのIndicationを購読
//     して、Body Fat Percentageと任意のWeightをデコードする。
#include <EspBle.h>

static constexpr const char *BODY_COMPOSITION_SERVICE_UUID = "181b";
static constexpr const char *BODY_COMPOSITION_MEASUREMENT_UUID = "2a9c";
static constexpr const char *BODY_COMPOSITION_FEATURE_UUID = "2a9b";

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
    ble.readCharacteristic(connection.id, BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_FEATURE_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 4)
    {
      ble.subscribe(result.connectionId, BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, false);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(BODY_COMPOSITION_MEASUREMENT_UUID) ||
        notification.value.length() < 4)
      return;
    const uint16_t flags = static_cast<uint8_t>(notification.value[0]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[1])) << 8);
    const uint16_t fatRaw = static_cast<uint8_t>(notification.value[2]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[3])) << 8);
    // en: Body Fat Percentage resolution is 0.1 % per LSB.
    // ja: Body Fat Percentageの分解能は1 LSBあたり0.1 %。
    Serial.printf("Body fat: %.1f %%", fatRaw * 0.1);
    // en: Weight (flag bit 10) is the only optional field this example emits, so
    //     it follows the mandatory Body Fat Percentage directly when present.
    // ja: この例が出す任意フィールドはWeight（flag bit 10）だけなので、存在すれば
    //     必須のBody Fat Percentageの直後に続く。
    if ((flags & 0x0400) && notification.value.length() >= 6)
    {
      const uint16_t weightRaw = static_cast<uint8_t>(notification.value[4]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[5])) << 8);
      const double weight = (flags & 0x01) ? weightRaw * 0.01 : weightRaw * 0.005;
      Serial.printf(", weight: %.3f %s", weight, (flags & 0x01) ? "lb" : "kg");
    }
    Serial.println();
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(BODY_COMPOSITION_SERVICE_UUID))
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
