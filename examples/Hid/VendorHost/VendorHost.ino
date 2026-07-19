#include <EspBle.h>

EspBle ble;
EspBleConnectionId connectionId = 0;

void setup()
{
  Serial.begin(115200);

  ble.hidHost().onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf("HID discovery: %s\n", result.success ? "ready" : result.detail.c_str());
  });
  ble.hidHost().onVendorInput([](const EspBleHidVendorInputEvent &event) {
    Serial.printf("Vendor Input report=%u length=%u data=", event.reportId,
      static_cast<unsigned>(event.rawLength));
    for (size_t index = 0; index < event.rawLength; ++index)
      Serial.printf("%s%02x", index == 0 ? "" : " ", event.rawData[index]);
    Serial.println();
  });

  EspBleConfig config;
  config.deviceName = "EspBle Vendor Host";
  config.preferredMtu = 100;
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    if (event.success) ble.hidHost().discover(event.connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &) { connectionId = 0; });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionId != 0 || !result.advertisesService("1812")) return;
    ble.scanner().stop();
    ble.connect(result);
  });
  ble.scanner().start();
  Serial.println("Send 'o' for Output or 'f' for Feature after discovery.");
}

void loop()
{
  if (Serial.available() > 0 && connectionId != 0)
  {
    const char command = Serial.read();
    if (command == 'o')
    {
      const uint8_t report[] = {'O', 'U', 'T', 3, 4, 5, 6, 7};
      Serial.printf("Output: %s\n", ble.hidHost().sendVendorOutput(
        connectionId, report, sizeof(report)) ? "sent" : "failed");
    }
    else if (command == 'f')
    {
      const uint8_t report[] = {'F', 'E', 'A', 'T', 4, 5, 6, 7};
      Serial.printf("Feature: %s\n", ble.hidHost().sendVendorFeature(
        connectionId, report, sizeof(report)) ? "sent" : "failed");
    }
  }
  ble.update();
  delay(1);
}
