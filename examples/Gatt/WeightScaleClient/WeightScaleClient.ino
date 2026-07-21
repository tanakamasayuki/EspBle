// en: WeightScaleClient - connect to a Weight Scale Service (0x181D), read
//     Weight Scale Feature, subscribe to Weight Measurement indications, and
//     decode the uint16 weight (0.005 kg resolution).
// ja: WeightScaleClient - Weight Scale Service（0x181D）へ接続し、Weight Scale
//     FeatureをRead、Weight MeasurementのIndicationを購読して、0.005 kg分解能の
//     uint16体重をデコードする。
#include <EspBle.h>

static constexpr const char *WEIGHT_SCALE_SERVICE_UUID = "181d";
static constexpr const char *WEIGHT_MEASUREMENT_UUID = "2a9d";
static constexpr const char *WEIGHT_SCALE_FEATURE_UUID = "2a9e";

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
    ble.readCharacteristic(connection.id, WEIGHT_SCALE_SERVICE_UUID, WEIGHT_SCALE_FEATURE_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 4)
    {
      ble.subscribe(result.connectionId, WEIGHT_SCALE_SERVICE_UUID, WEIGHT_MEASUREMENT_UUID, false);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(WEIGHT_MEASUREMENT_UUID) ||
        notification.value.length() < 3)
      return;
    const uint8_t flags = static_cast<uint8_t>(notification.value[0]);
    const uint16_t raw = static_cast<uint8_t>(notification.value[1]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[2])) << 8);
    // en: SI weight resolution is 0.005 kg per LSB; Imperial is 0.01 lb.
    // ja: SIの体重分解能は1 LSBあたり0.005 kg、Imperialは0.01 lb。
    const double weight = (flags & 0x01) ? raw * 0.01 : raw * 0.005;
    Serial.printf("Weight: %.3f %s\n", weight, (flags & 0x01) ? "lb" : "kg");
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(WEIGHT_SCALE_SERVICE_UUID))
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
