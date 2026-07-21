// glucose peer_device: EspBle GATT server for the standard Glucose Service,
// exercising the Record Access Control Point (RACP) procedure. When the client
// writes "Report Stored Records (all)" to the RACP, the server notifies one
// Glucose Measurement (sequence number, base time, SFLOAT concentration) and
// then indicates the RACP response. Because BLE sends are single-in-flight, the
// measurement notify and the RACP indicate are sequenced from onSent.
#include <EspBle.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *GLUCOSE_SERVICE_UUID = "1808";
static constexpr const char *GLUCOSE_MEASUREMENT_UUID = "2a18";
static constexpr const char *GLUCOSE_FEATURE_UUID = "2a51";
static constexpr const char *RACP_UUID = "2a52";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t feature[2] = {0x00, 0x00};

enum RacpState
{
  RACP_IDLE,
  RACP_SEND_MEASUREMENT,
  RACP_SEND_RESPONSE,
};
volatile RacpState racpState = RACP_IDLE;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static void buildMeasurement(uint8_t *out)
{
  // flags 0x02: Glucose Concentration, Type and Sample Location present (kg/L).
  out[0] = 0x02;
  out[1] = 0x01; // sequence number uint16 LE = 1
  out[2] = 0x00;
  out[3] = 0xEA; // base time: year 2026 (0x07EA) LE
  out[4] = 0x07;
  out[5] = 7;    // month
  out[6] = 21;   // day
  out[7] = 12;   // hour
  out[8] = 0;    // minute
  out[9] = 0;    // second
  espBleWriteMedicalSFloatLE(&out[10], 99, 0); // concentration SFLOAT = 99
  out[12] = 0x11; // type = Capillary Whole blood (1), location = Finger (1)
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  EspBleGattCharacteristicConfig racpConfig;
  racpConfig.writable = true;
  racpConfig.indicatable = true;
  auto &server = ble.gattServer();
  if (!server.addService(GLUCOSE_SERVICE_UUID) ||
      !server.addCharacteristic(GLUCOSE_SERVICE_UUID, GLUCOSE_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(GLUCOSE_SERVICE_UUID, GLUCOSE_FEATURE_UUID, featureConfig) ||
      !server.addCharacteristic(GLUCOSE_SERVICE_UUID, RACP_UUID, racpConfig) ||
      !server.setValue(GLUCOSE_SERVICE_UUID, GLUCOSE_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(RACP_UUID))
      return;
    // Report Stored Records (opcode 1), operator All records (1).
    const bool reportAll = write.value.length() == 2 &&
      static_cast<uint8_t>(write.value[0]) == 0x01 &&
      static_cast<uint8_t>(write.value[1]) == 0x01;
    Serial.printf("RACP_WRITE length=%u reportAll=%u context=%s\n",
      static_cast<unsigned>(write.value.length()), reportAll ? 1 : 0, contextName());
    if (!reportAll)
      return;
    uint8_t measurement[13];
    buildMeasurement(measurement);
    racpState = RACP_SEND_MEASUREMENT;
    ble.gattServer().notify(GLUCOSE_SERVICE_UUID, GLUCOSE_MEASUREMENT_UUID, measurement, sizeof(measurement));
  });
  server.onSent([](const EspBleGattSendResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID) &&
        racpState == RACP_SEND_MEASUREMENT)
    {
      racpState = RACP_SEND_RESPONSE;
      // RACP response: Response Code (0x06), operator 0, request opcode 1, success 1.
      const uint8_t response[4] = {0x06, 0x00, 0x01, 0x01};
      ble.gattServer().indicate(GLUCOSE_SERVICE_UUID, RACP_UUID, response, sizeof(response));
    }
    else if (result.characteristicUuid.equalsIgnoreCase(RACP_UUID) &&
             racpState == RACP_SEND_RESPONSE)
    {
      racpState = RACP_IDLE;
      Serial.printf("RACP_DONE success=%u context=%s\n", result.success ? 1 : 0, contextName());
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Glucose Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Glucose Peer");
  ble.advertising().addServiceUuid(GLUCOSE_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
