// disconnect_reason peer_device: EspBle GATT server (peripheral) that advertises
// a marker service, accepts a connection, and terminates it on command. It
// reports its own disconnect reason from EspBleConnection::disconnectReason in
// onDisconnected. As the initiator of the disconnect, it should see a non-zero
// "local host terminated" reason that differs from the reason the remote DUT
// sees, proving the reason code is captured on both the server and client paths.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *SERVICE_UUID = "1815"; // Automation IO, advertised marker

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  // A minimal readable characteristic so the marker service is non-empty.
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
  config.deviceName = "EspBle Disc Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("PEER_CONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    Serial.printf("PEER_DISCONNECTED id=%u reason=%d context=%s\n",
      static_cast<unsigned>(connection.id), connection.disconnectReason, contextName());
  });
  ble.advertising().setName("EspBle Disc Peer");
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
    else if (command == 'x' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "PEER_DISCONNECT_REQUESTED" : "PEER_DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
