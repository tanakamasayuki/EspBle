// phy_update DUT: EspBle GATT client that connects to the peer, reports the
// initial LE PHY from onConnected, requests the 2M PHY via updatePhy(), and
// reports the negotiated PHY delivered to onPhyUpdated(). ESP32-S3 supports the
// 2M PHY, so both peers should converge on tx/rx PHY 2.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *SERVICE_UUID = "1815"; // advertised marker

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
    Serial.printf("CONNECTED id=%u tx_phy=%u rx_phy=%u context=%s\n",
      static_cast<unsigned>(connection.id), connection.txPhy, connection.rxPhy, contextName());
  });
  ble.onPhyUpdated([](const EspBleConnection &connection) {
    Serial.printf("PHY_UPDATED tx_phy=%u rx_phy=%u context=%s\n",
      connection.txPhy, connection.rxPhy, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(SERVICE_UUID))
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
    else if (command == 'p' && connectionId != 0)
    {
      const bool accepted = ble.updatePhy(connectionId, EspBle::Phy2MMask, EspBle::Phy2MMask);
      Serial.println(accepted ? "PHY_REQUESTED" : "PHY_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
