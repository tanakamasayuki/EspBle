// en: GlucoseServer - standard Glucose Service (0x1808) with the Record Access
//     Control Point (RACP). When a client writes "Report Stored Records (all)",
//     the server notifies one Glucose Measurement (sequence, base time, SFLOAT
//     concentration) and then indicates the RACP response. Sends are queued, so
//     onSent here is a deliberate choice: the RACP "operation complete" response
//     is indicated only after the measurement is confirmed delivered.
// ja: GlucoseServer - Record Access Control Point（RACP）付きの標準Glucose
//     Service（0x1808）。Clientが「Report Stored Records（all）」を書き込むと、
//     Glucose Measurement（sequence、base time、SFLOAT濃度）を1件Notifyし、続けて
//     RACP応答をIndicateする。送信はFIFOにqueueされるので、ここでonSentを使うのは
//     意図的で、measurementの配送完了を待ってからRACPの「完了」応答をIndicateする。
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *GLUCOSE_SERVICE_UUID = "1808";
static constexpr const char *GLUCOSE_MEASUREMENT_UUID = "2a18";
static constexpr const char *GLUCOSE_FEATURE_UUID = "2a51";
static constexpr const char *RACP_UUID = "2a52";

EspBle ble;
EspBleGattService glucoseServiceService;
EspBleGattCharacteristic glucoseMeasurementCharacteristic;
EspBleGattCharacteristic glucoseFeatureCharacteristic;
EspBleGattCharacteristic racpCharacteristic;
const uint8_t feature[2] = {0x00, 0x00};
enum RacpState { RACP_IDLE, RACP_SEND_MEASUREMENT, RACP_SEND_RESPONSE };
RacpState racpState = RACP_IDLE;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  EspBleGattCharacteristicConfig racpConfig;
  racpConfig.writable = true;
  racpConfig.indicatable = true;
  auto &server = ble.gattServer();
  glucoseServiceService = server.addService(GLUCOSE_SERVICE_UUID);
  glucoseMeasurementCharacteristic = server.addCharacteristic(glucoseServiceService, GLUCOSE_MEASUREMENT_UUID, measurementConfig);
  glucoseFeatureCharacteristic = server.addCharacteristic(glucoseServiceService, GLUCOSE_FEATURE_UUID, featureConfig);
  racpCharacteristic = server.addCharacteristic(glucoseServiceService, RACP_UUID, racpConfig);
  server.setValue(glucoseFeatureCharacteristic, feature, sizeof(feature));

  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(RACP_UUID))
      return;
    if (write.value.length() == 2 && static_cast<uint8_t>(write.value[0]) == 0x01)
    {
      uint8_t measurement[13];
      measurement[0] = 0x02; // concentration + type/location present (kg/L)
      measurement[1] = 0x01; // sequence number = 1
      measurement[2] = 0x00;
      measurement[3] = 0xEA; // base time year 2026
      measurement[4] = 0x07;
      measurement[5] = 7;
      measurement[6] = 21;
      measurement[7] = 12;
      measurement[8] = 0;
      measurement[9] = 0;
      espBleWriteMedicalSFloatLE(&measurement[10], 99, 0);
      measurement[12] = 0x11;
      racpState = RACP_SEND_MEASUREMENT;
      ble.gattServer().notify(glucoseMeasurementCharacteristic, measurement, sizeof(measurement));
    }
  });
  server.onSent([](const EspBleGattSendResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID) &&
        racpState == RACP_SEND_MEASUREMENT)
    {
      racpState = RACP_SEND_RESPONSE;
      const uint8_t response[4] = {0x06, 0x00, 0x01, 0x01};
      ble.gattServer().indicate(racpCharacteristic, response, sizeof(response));
    }
    else if (result.characteristicUuid.equalsIgnoreCase(RACP_UUID))
    {
      racpState = RACP_IDLE;
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Glucose";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(GLUCOSE_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  ble.update();
  delay(1);
}
