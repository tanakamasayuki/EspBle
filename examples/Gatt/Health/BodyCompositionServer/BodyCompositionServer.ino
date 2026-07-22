// en: BodyCompositionServer - standard Body Composition Service (0x181B). Body
//     Composition Measurement (0x2A9C) is indicated with uint16 flags, the
//     mandatory Body Fat Percentage (0.1 %/LSB), and optional fields; Body
//     Composition Feature (0x2A9B) is a readable uint32.
// ja: BodyCompositionServer - 標準Body Composition Service（0x181B）。Body
//     Composition Measurement（0x2A9C）をuint16 flags、必須のBody Fat Percentage
//     （0.1 %/LSB）、任意フィールド付きでIndicateし、Body Composition Feature
//     （0x2A9B）はuint32としてReadできる。
#include <EspBle.h>

static constexpr const char *BODY_COMPOSITION_SERVICE_UUID = "181b";
static constexpr const char *BODY_COMPOSITION_MEASUREMENT_UUID = "2a9c";
static constexpr const char *BODY_COMPOSITION_FEATURE_UUID = "2a9b";

EspBle ble;
const uint8_t feature[4] = {0x00, 0x02, 0x00, 0x00}; // en: bit 9 = Weight supported / ja: bit 9 = Weight対応
unsigned long lastUpdate = 0;
uint16_t rawFat = 275; // en: 27.5 % / ja: 27.5 %

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.indicatable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  server.addService(BODY_COMPOSITION_SERVICE_UUID);
  server.addCharacteristic(BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, measurementConfig);
  server.addCharacteristic(BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_FEATURE_UUID, featureConfig);
  server.setValue(BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_FEATURE_UUID, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle Body Composition";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(BODY_COMPOSITION_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 3 seconds, indicate body fat drifting near 27.5 % plus a fixed
  //     70.000 kg weight (flag bit 10 set).
  // ja: 3秒ごとに、27.5 %付近で変化する体脂肪率と固定の70.000 kg体重（flag bit
  //     10をセット）をIndicateする。
  if (millis() - lastUpdate >= 3000)
  {
    lastUpdate = millis();
    rawFat += 1; // en: +0.1 % / ja: +0.1 %
    if (rawFat > 285)
      rawFat = 275;

    const uint16_t flags = 0x0400; // en: SI units, Weight present / ja: SI単位、Weightあり
    const uint16_t rawWeight = 14000; // en: 70.000 kg / ja: 70.000 kg
    uint8_t measurement[6];
    measurement[0] = static_cast<uint8_t>(flags & 0xFF);
    measurement[1] = static_cast<uint8_t>((flags >> 8) & 0xFF);
    measurement[2] = static_cast<uint8_t>(rawFat & 0xFF);
    measurement[3] = static_cast<uint8_t>((rawFat >> 8) & 0xFF);
    measurement[4] = static_cast<uint8_t>(rawWeight & 0xFF);
    measurement[5] = static_cast<uint8_t>((rawWeight >> 8) & 0xFF);
    ble.gattServer().setValue(
      BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, measurement, sizeof(measurement));
    ble.gattServer().indicate(
      BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, measurement, sizeof(measurement));
  }

  ble.update();
  delay(1);
}
