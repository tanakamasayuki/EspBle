// en: PulseOximeterServer - standard Pulse Oximeter Service / PLX (0x1822). PLX
//     Spot-Check Measurement (0x2A5E) is indicated with SpO2 and pulse rate as
//     IEEE-11073 16-bit SFLOATs; PLX Features (0x2A60) is readable.
// ja: PulseOximeterServer - 標準Pulse Oximeter Service / PLX（0x1822）。PLX
//     Spot-Check Measurement（0x2A5E）をSpO2とpulse rateのIEEE-11073 16-bit
//     SFLOATでIndicateし、PLX Features（0x2A60）はReadできる。
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *PLX_SERVICE_UUID = "1822";
static constexpr const char *PLX_SPOT_CHECK_UUID = "2a5e";
static constexpr const char *PLX_FEATURES_UUID = "2a60";

EspBle ble;
const uint8_t features[2] = {0x03, 0x00};
int spo2 = 98;
unsigned long lastUpdate = 0;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig spotCheckConfig;
  spotCheckConfig.indicatable = true;
  EspBleGattCharacteristicConfig featuresConfig;
  featuresConfig.readable = true;
  auto &server = ble.gattServer();
  server.addService(PLX_SERVICE_UUID);
  server.addCharacteristic(PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, spotCheckConfig);
  server.addCharacteristic(PLX_SERVICE_UUID, PLX_FEATURES_UUID, featuresConfig);
  server.setValue(PLX_SERVICE_UUID, PLX_FEATURES_UUID, features, sizeof(features));

  EspBleConfig config;
  config.deviceName = "EspBle Pulse Oximeter";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(PLX_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 3 seconds, indicate SpO2 (95-99 %) and a 60 bpm pulse rate.
  // ja: 3秒ごとに SpO2（95〜99 %）と 60 bpm の pulse rate をIndicateする。
  if (millis() - lastUpdate >= 3000)
  {
    lastUpdate = millis();
    spo2 += 1;
    if (spo2 > 99)
      spo2 = 95;

    uint8_t measurement[5];
    measurement[0] = 0x00; // en: no optional fields / ja: optionalなし
    espBleWriteMedicalSFloatLE(&measurement[1], static_cast<int16_t>(spo2), 0);
    espBleWriteMedicalSFloatLE(&measurement[3], 60, 0);
    ble.gattServer().setValue(PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, measurement, sizeof(measurement));
    ble.gattServer().indicate(PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, measurement, sizeof(measurement));
  }

  ble.update();
  delay(1);
}
