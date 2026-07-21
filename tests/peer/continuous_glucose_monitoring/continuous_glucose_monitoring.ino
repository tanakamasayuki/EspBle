// continuous_glucose_monitoring DUT: EspBle GATT client for the CGM Service. It
// reads CGM Feature (verifying its E2E-CRC and decoding the feature bits and
// type/sample location) and subscribes to CGM Measurement notifications,
// verifying each measurement's E2E-CRC and decoding the SFLOAT glucose
// concentration and time offset.
#include <EspBle.h>
#include <EspBleCgmCrc.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static constexpr const char *CGM_SERVICE_UUID = "181f";
static constexpr const char *CGM_MEASUREMENT_UUID = "2aa7";
static constexpr const char *CGM_FEATURE_UUID = "2aa8";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static void copyToBuffer(const String &value, uint8_t *buffer, size_t length)
{
  for (size_t i = 0; i < length; ++i)
    buffer[i] = static_cast<uint8_t>(value[i]);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();
  if (!ble.begin())
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    const bool accepted = ble.readCharacteristic(connection.id, CGM_SERVICE_UUID, CGM_FEATURE_UUID);
    Serial.println(accepted ? "FEATURE_READ_REQUESTED" : "FEATURE_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(CGM_FEATURE_UUID))
      return;
    // CGM Feature: Feature(uint24) + Type/Sample Location(uint8) + E2E-CRC(2).
    const size_t length = result.value.length();
    bool valid = result.success && length == 6;
    bool crcOk = false;
    uint32_t feature = 0;
    uint8_t typeLocation = 0;
    if (valid)
    {
      uint8_t buffer[6];
      copyToBuffer(result.value, buffer, length);
      crcOk = espBleCgmVerifyCrc(buffer, length);
      feature = static_cast<uint32_t>(buffer[0]) |
        (static_cast<uint32_t>(buffer[1]) << 8) |
        (static_cast<uint32_t>(buffer[2]) << 16);
      typeLocation = buffer[3];
    }
    Serial.printf("FEATURE_READ valid=%u crc_ok=%u feature=%06x type_location=%02x context=%s\n",
      valid ? 1 : 0, crcOk ? 1 : 0, static_cast<unsigned>(feature), typeLocation, contextName());
    if (valid && crcOk)
    {
      const bool accepted = ble.subscribe(result.connectionId, CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID);
      Serial.println(accepted ? "CGM_SUBSCRIBE_REQUESTED" : "CGM_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("CGM_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("CGM_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(CGM_MEASUREMENT_UUID))
      return;
    // CGM Measurement: Size(1) + Flags(1) + Glucose SFLOAT(2) + Time Offset
    // uint16(2) + E2E-CRC(2).
    const size_t length = notification.value.length();
    const bool valid = length == 8;
    bool crcOk = false;
    uint8_t size = 0, flags = 0;
    long glucose = 0;
    unsigned timeOffset = 0;
    if (valid)
    {
      uint8_t buffer[8];
      copyToBuffer(notification.value, buffer, length);
      crcOk = espBleCgmVerifyCrc(buffer, length);
      size = buffer[0];
      flags = buffer[1];
      const uint8_t sfloat[2] = {buffer[2], buffer[3]};
      glucose = lround(espBleReadMedicalSFloatLE(sfloat));
      timeOffset = static_cast<unsigned>(buffer[4]) | (static_cast<unsigned>(buffer[5]) << 8);
    }
    Serial.printf(
      "CGM_MEASUREMENT valid=%u crc_ok=%u size=%u flags=%02x glucose=%ld time_offset=%u context=%s\n",
      valid ? 1 : 0, crcOk ? 1 : 0, size, flags, glucose, timeOffset, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(CGM_SERVICE_UUID))
      return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
    Serial.println(connectionRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's' && !connectionRequested)
    {
      EspBleScanConfig scan;
      scan.active = true;
      Serial.println(ble.scanner().start(scan) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'u' && connectionId != 0)
    {
      const bool accepted = ble.unsubscribe(connectionId, CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID);
      Serial.println(accepted ? "CGM_UNSUBSCRIBE_REQUESTED" : "CGM_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
