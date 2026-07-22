// Manual (3-board) test: one Central holds two simultaneous Peripheral
// connections and auto-reconnects one of them while the other stays up.
//
// Boards: this Central on the host port, peer_device (Peripheral A) and
// peer_device2 (Peripheral B). Peripheral B is only present when the third
// board is attached, so this lives under tests/manual/ and the pytest skips
// when that board's port is not configured.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *SERVICE_A_UUID = "6d756c74-6900-4100-9003-636f6e6e4141";
static constexpr const char *CHAR_A_UUID = "6d756c74-6900-4100-9003-636f6e6e4142";
static constexpr const char *SERVICE_B_UUID = "6d756c74-6900-4200-9003-636f6e6e4241";
static constexpr const char *CHAR_B_UUID = "6d756c74-6900-4200-9003-636f6e6e4242";

EspBle ble;
TaskHandle_t loopTask = nullptr;
bool connectionRequested = false;
char pendingLabel = '?';
char targetLabel = '?';

String addressA;
String addressB;
EspBleConnectionId connectionA = 0;
EspBleConnectionId connectionB = 0;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static char labelForConnection(EspBleConnectionId id)
{
  if (id != 0 && id == connectionA) return 'A';
  if (id != 0 && id == connectionB) return 'B';
  return '?';
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleConfig config;
  config.deviceName = "EspBle Multi Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.setAutoReconnect(true);

  ble.onConnected([](const EspBleConnection &connection) {
    char label = '?';
    if (addressA.equalsIgnoreCase(connection.peerAddress) ||
        (addressA.length() == 0 && pendingLabel == 'A'))
    {
      label = 'A';
      addressA = connection.peerAddress;
      connectionA = connection.id;
    }
    else if (addressB.equalsIgnoreCase(connection.peerAddress) ||
             (addressB.length() == 0 && pendingLabel == 'B'))
    {
      label = 'B';
      addressB = connection.peerAddress;
      connectionB = connection.id;
    }
    connectionRequested = false;
    pendingLabel = '?';
    Serial.printf("CENTRAL_CONNECTED label=%c id=%u count=%u\n",
      label, static_cast<unsigned>(connection.id),
      static_cast<unsigned>(ble.connectionCount()));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    const char label = labelForConnection(connection.id);
    if (connection.id == connectionA) connectionA = 0;
    if (connection.id == connectionB) connectionB = 0;
    Serial.printf("CENTRAL_DISCONNECTED label=%c count=%u\n",
      label, static_cast<unsigned>(ble.connectionCount()));
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("SUBSCRIBED label=%c success=%u context=%s\n",
      labelForConnection(result.connectionId), result.success ? 1 : 0, callbackContext());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf("RECEIVED label=%c value=%s context=%s\n",
      labelForConnection(notification.connectionId), notification.value.c_str(), callbackContext());
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested) return;
    const char *serviceUuid = targetLabel == 'A' ? SERVICE_A_UUID
                            : targetLabel == 'B' ? SERVICE_B_UUID : nullptr;
    if (serviceUuid == nullptr || !scanResult.advertisesService(serviceUuid)) return;
    ble.scanner().stop();
    pendingLabel = targetLabel;
    connectionRequested = ble.connect(scanResult);
    Serial.printf("CONNECT_REQUESTED label=%c ok=%u\n", targetLabel, connectionRequested ? 1 : 0);
  });
}

static void startScanFor(char label)
{
  targetLabel = label;
  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '1' && !connectionRequested)
    {
      startScanFor('A');
    }
    else if (command == '2' && !connectionRequested)
    {
      startScanFor('B');
    }
    else if (command == 'p' && connectionA != 0)
    {
      Serial.println(ble.subscribe(connectionA, SERVICE_A_UUID, CHAR_A_UUID, true)
        ? "SUBSCRIBE_A_REQUESTED" : "SUBSCRIBE_A_FAILED");
    }
    else if (command == 'q' && connectionB != 0)
    {
      Serial.println(ble.subscribe(connectionB, SERVICE_B_UUID, CHAR_B_UUID, true)
        ? "SUBSCRIBE_B_REQUESTED" : "SUBSCRIBE_B_FAILED");
    }
  }

  ble.update();
  delay(1);
}
