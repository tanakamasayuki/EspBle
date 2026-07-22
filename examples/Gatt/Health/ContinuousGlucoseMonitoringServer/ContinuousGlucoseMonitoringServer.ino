// en: ContinuousGlucoseMonitoringServer - standard CGM Service (0x181F). CGM
//     Feature (0x2AA8) is a readable value protected by an E2E-CRC; CGM
//     Measurement (0x2AA7) is notified with an SFLOAT glucose concentration, a
//     time offset, and an appended E2E-CRC (CRC-16/MCRF4XX) from EspBleCgmCrc.h.
// ja: ContinuousGlucoseMonitoringServer - 標準CGM Service（0x181F）。CGM Feature
//     （0x2AA8）はE2E-CRCで保護されたread値、CGM Measurement（0x2AA7）はSFLOAT
//     血糖値、time offset、末尾のE2E-CRC（CRC-16/MCRF4XX、EspBleCgmCrc.h）付きで
//     Notifyする。
#include <EspBle.h>
#include <EspBleCgmCrc.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *CGM_SERVICE_UUID = "181f";
static constexpr const char *CGM_MEASUREMENT_UUID = "2aa7";
static constexpr const char *CGM_FEATURE_UUID = "2aa8";

EspBle ble;
unsigned long lastUpdate = 0;
uint16_t timeOffset = 5; // en: minutes since session start / ja: セッション開始からの分
int glucose = 100;       // en: mg/dL / ja: mg/dL

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  uint8_t feature[6];
  feature[0] = 0x00; // en: bit 12 = E2E-CRC supported (0x001000) / ja: bit 12 = E2E-CRC対応
  feature[1] = 0x10;
  feature[2] = 0x00;
  feature[3] = 0x11; // en: Type = Capillary Whole blood, Location = Finger / ja: Type=毛細血、Location=指
  espBleCgmAppendCrc(feature, 4);
  server.addService(CGM_SERVICE_UUID);
  server.addCharacteristic(CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID, measurementConfig);
  server.addCharacteristic(CGM_SERVICE_UUID, CGM_FEATURE_UUID, featureConfig);
  server.setValue(CGM_SERVICE_UUID, CGM_FEATURE_UUID, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle CGM";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(CGM_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 3 seconds, notify a CGM Measurement with a fresh time offset and a
  //     glucose value drifting around 100 mg/dL, each protected by an E2E-CRC.
  // ja: 3秒ごとに、新しいtime offsetと100 mg/dL付近で変化する血糖値を持つCGM
  //     MeasurementをE2E-CRC付きでNotifyする。
  if (millis() - lastUpdate >= 3000)
  {
    lastUpdate = millis();
    timeOffset += 5;
    glucose += 1;
    if (glucose > 110)
      glucose = 100;

    uint8_t measurement[8];
    measurement[0] = 8;    // en: Size / ja: Size
    measurement[1] = 0x00; // en: Flags / ja: Flags
    espBleWriteMedicalSFloatLE(&measurement[2], glucose, 0);
    measurement[4] = static_cast<uint8_t>(timeOffset & 0xFF);
    measurement[5] = static_cast<uint8_t>((timeOffset >> 8) & 0xFF);
    espBleCgmAppendCrc(measurement, 6);
    ble.gattServer().setValue(CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID, measurement, sizeof(measurement));
    ble.gattServer().notify(CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID, measurement, sizeof(measurement));
  }

  ble.update();
  delay(1);
}
