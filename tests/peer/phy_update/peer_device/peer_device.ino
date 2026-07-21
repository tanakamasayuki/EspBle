// phy_update peer_device: EspBle GATT server (peripheral) that advertises a
// marker service and reports LE PHY updates. When the central requests the 2M
// PHY, both peers receive a BLE_GAP_EVENT_PHY_UPDATE_COMPLETE, so this
// peripheral's onPhyUpdated should report the same negotiated PHY as the central.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *SERVICE_UUID = "1815"; // advertised marker

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig markerConfig;
  markerConfig.readable = true;
  auto &server = ble.gattServer();
  const uint8_t marker = 0;
  if (!server.addService(SERVICE_UUID) ||
      !server.addCharacteristic(SERVICE_UUID, "2ae2", markerConfig) ||
      !server.setValue(SERVICE_UUID, "2ae2", &marker, sizeof(marker)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle PHY Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("PEER_CONNECTED id=%u tx_phy=%u rx_phy=%u context=%s\n",
      static_cast<unsigned>(connection.id), connection.txPhy, connection.rxPhy, contextName());
  });
  ble.onPhyUpdated([](const EspBleConnection &connection) {
    Serial.printf("PEER_PHY_UPDATED tx_phy=%u rx_phy=%u context=%s\n",
      connection.txPhy, connection.rxPhy, contextName());
  });
  ble.advertising().setName("EspBle PHY Peer");
  ble.advertising().addServiceUuid(SERVICE_UUID);
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
