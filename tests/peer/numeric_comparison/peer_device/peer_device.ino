// numeric_comparison peer_device: EspBle GATT server using LE Secure Connections
// Numeric Comparison (DisplayYesNo, MITM). It advertises a marker service,
// surfaces the comparison value via onNumericComparison, and confirms it on
// command; pairing then completes authenticated and bonded.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *MARKER_SERVICE_UUID = "1815"; // advertised marker

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
  markerConfig.authenticatedRead = true;
  auto &server = ble.gattServer();
  const uint8_t marker = 0;
  server.addService(MARKER_SERVICE_UUID);
  server.addCharacteristic(MARKER_SERVICE_UUID, "2ae2", markerConfig);
  server.setValue(MARKER_SERVICE_UUID, "2ae2", &marker, sizeof(marker));

  EspBleConfig config;
  config.deviceName = "EspBle NumCmp Peer";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  config.security.ioCapability = EspBleSecurityIoCapability::DisplayYesNo;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onNumericComparison([](const EspBlePasskeyDisplayed &event) {
    Serial.printf("PERIPHERAL_NUMCMP id=%u value=%06u context=%s\n",
      static_cast<unsigned>(event.connection.id),
      static_cast<unsigned>(event.passkey), contextName());
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "PERIPHERAL_SECURITY success=%u encrypted=%u authenticated=%u bonded=%u key=%u context=%s\n",
      event.success ? 1 : 0, event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0, event.connection.bonded ? 1 : 0,
      event.connection.encryptionKeySize, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle NumCmp Peer");
  advertising.addServiceUuid(MARKER_SERVICE_UUID);
  advertising.start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("PERIPHERAL_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'y')
    {
      Serial.printf("PERIPHERAL_CONFIRM accepted=%u\n", ble.confirmNumericComparison(true) ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
