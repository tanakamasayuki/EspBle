#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *DEVICE_INFORMATION_SERVICE_UUID = "180a";
static constexpr const char *CHARACTERISTIC_UUIDS[] = {"2a29", "2a24", "2a26", "2a50"};

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
size_t readIndex = 0;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static bool requestRead(EspBleConnectionId id)
{
  return ble.readCharacteristic(
    id, DEVICE_INFORMATION_SERVICE_UUID, CHARACTERISTIC_UUIDS[readIndex]);
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
    readIndex = 0;
    Serial.println(requestRead(connection.id) ? "DIS_READ_REQUESTED" : "DIS_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("DIS_READ_FAILED index=%u context=%s\n",
        static_cast<unsigned>(readIndex), contextName());
      return;
    }
    if (readIndex < 3)
    {
      Serial.printf("DIS_TEXT index=%u value=%s context=%s\n",
        static_cast<unsigned>(readIndex), result.value.c_str(), contextName());
    }
    else
    {
      const bool valid = result.value.length() == 7;
      const uint16_t vendorId = valid ? static_cast<uint8_t>(result.value[1]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[2])) << 8) : 0;
      const uint16_t productId = valid ? static_cast<uint8_t>(result.value[3]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[4])) << 8) : 0;
      const uint16_t version = valid ? static_cast<uint8_t>(result.value[5]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[6])) << 8) : 0;
      Serial.printf(
        "DIS_PNP valid=%u source=%u vendor=%04x product=%04x version=%04x context=%s\n",
        valid ? 1 : 0,
        valid ? static_cast<uint8_t>(result.value[0]) : 0,
        vendorId, productId, version, contextName());
    }

    ++readIndex;
    if (readIndex < 4)
    {
      Serial.println(requestRead(result.connectionId)
        ? "DIS_READ_REQUESTED" : "DIS_READ_REQUEST_FAILED");
    }
    else
    {
      Serial.printf("DIS_COMPLETE context=%s\n", contextName());
    }
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested ||
        !result.advertisesService(DEVICE_INFORMATION_SERVICE_UUID)) return;
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
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId)
        ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
