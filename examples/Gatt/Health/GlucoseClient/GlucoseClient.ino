// en: GlucoseClient - connect to a Glucose Service (0x1808), subscribe to
//     Glucose Measurement notifications and the Record Access Control Point
//     (RACP) indications, then write "Report Stored Records (all)" and print the
//     decoded measurements and the RACP response.
// ja: GlucoseClient - Glucose Service（0x1808）へ接続し、Glucose Measurementの
//     NotificationとRecord Access Control Point（RACP）のIndicationを購読し、
//     「Report Stored Records（all）」を書き込んで、デコードした測定値とRACP応答を
//     表示する。
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *GLUCOSE_SERVICE_UUID = "1808";
static constexpr const char *GLUCOSE_MEASUREMENT_UUID = "2a18";
static constexpr const char *RACP_UUID = "2a52";

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
    ble.subscribe(connection.id, GLUCOSE_SERVICE_UUID, GLUCOSE_MEASUREMENT_UUID);
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID) && result.success)
    {
      // RACP uses indications (notifications = false).
      ble.subscribe(result.connectionId, GLUCOSE_SERVICE_UUID, RACP_UUID, false);
    }
    else if (result.characteristicUuid.equalsIgnoreCase(RACP_UUID) && result.success)
    {
      // Report Stored Records (opcode 1), operator All records (1).
      const uint8_t request[2] = {0x01, 0x01};
      ble.writeCharacteristic(result.connectionId, GLUCOSE_SERVICE_UUID, RACP_UUID, request, sizeof(request), true);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID) &&
        notification.value.length() >= 13)
    {
      const String &value = notification.value;
      const uint16_t sequence = static_cast<uint8_t>(value[1]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(value[2])) << 8);
      const uint8_t bytes[2] = {static_cast<uint8_t>(value[10]), static_cast<uint8_t>(value[11])};
      Serial.printf("Glucose record #%u: %.0f\n", sequence, espBleReadMedicalSFloatLE(bytes));
    }
    else if (notification.characteristicUuid.equalsIgnoreCase(RACP_UUID) &&
             notification.value.length() >= 4)
    {
      Serial.printf("RACP response: request=%u status=%u\n",
        static_cast<uint8_t>(notification.value[2]), static_cast<uint8_t>(notification.value[3]));
    }
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(GLUCOSE_SERVICE_UUID))
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
