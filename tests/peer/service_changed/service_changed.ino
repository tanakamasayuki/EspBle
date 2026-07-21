// service_changed DUT: EspBle GATT client that subscribes to the standard GATT
// Service Changed characteristic (Generic Attribute service 0x1801,
// characteristic 0x2A05) and decodes the changed attribute-handle range when the
// server indicates it. This validates client-side handling of Service Changed,
// which real peripherals use to tell clients to rediscover.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *MARKER_SERVICE_UUID = "1815"; // advertised marker
static constexpr const char *GATT_SERVICE_UUID = "1801";
static constexpr const char *SERVICE_CHANGED_UUID = "2a05";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
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
    // Service Changed is an indication: subscribe with notifications = false.
    const bool accepted = ble.subscribe(connection.id, GATT_SERVICE_UUID, SERVICE_CHANGED_UUID, false);
    Serial.println(accepted ? "SC_SUBSCRIBE_REQUESTED" : "SC_SUBSCRIBE_REQUEST_FAILED");
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("SC_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("SC_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(SERVICE_CHANGED_UUID))
      return;
    const bool valid = notification.value.length() == 4;
    unsigned start = 0, end = 0;
    if (valid)
    {
      start = static_cast<uint8_t>(notification.value[0]) |
        (static_cast<unsigned>(static_cast<uint8_t>(notification.value[1])) << 8);
      end = static_cast<uint8_t>(notification.value[2]) |
        (static_cast<unsigned>(static_cast<uint8_t>(notification.value[3])) << 8);
    }
    Serial.printf("SC_INDICATION valid=%u indication=%u start=%u end=%u context=%s\n",
      valid ? 1 : 0, notification.indication ? 1 : 0, start, end, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(MARKER_SERVICE_UUID))
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
      const bool accepted = ble.unsubscribe(connectionId, GATT_SERVICE_UUID, SERVICE_CHANGED_UUID);
      Serial.println(accepted ? "SC_UNSUBSCRIBE_REQUESTED" : "SC_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
