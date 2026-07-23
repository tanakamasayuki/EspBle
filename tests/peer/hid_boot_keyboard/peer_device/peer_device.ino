// Emulates a boot-style BLE keyboard WITHOUT report IDs using the generic
// GATT Server API: HID Service with a Report Map and a single notifiable
// Input Report characteristic, no Report Reference descriptor, no Battery
// Service. Lets tests exercise real-keyboard shapes the EspBle HID Keyboard
// Device profile cannot produce (missing descriptor, arbitrary payload
// lengths).
#include <EspBle.h>

EspBle ble;
EspBleConnectionId connectionId = 0;

// HID 1.11 Appendix E.6 boot keyboard report map (no Report ID).
const uint8_t bootReportMap[] = {
  0x05, 0x01, 0x09, 0x06, 0xa1, 0x01,
  0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
  0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
  0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
  0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
  0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
  0xc0,
};

static bool notifyReport(const uint8_t *data, size_t length)
{
  return ble.gattServer().notify("1812", "2a4d", data, length);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.printf("DEVICE_BOOT heap=%u\n", static_cast<unsigned>(ESP.getFreeHeap()));

  auto &server = ble.gattServer();
  EspBleGattCharacteristicConfig readable;
  readable.readable = true;
  EspBleGattCharacteristicConfig inputConfig;
  inputConfig.readable = true;
  inputConfig.notifiable = true;
  if (!server.addService("1812") ||
      !server.addCharacteristic("1812", "2a4b", readable) ||
      !server.addCharacteristic("1812", "2a4d", inputConfig) ||
      !server.setValue("1812", "2a4b", bootReportMap, sizeof(bootReportMap)))
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("DEVICE_SENT success=%u detail=%s\n",
      result.success ? 1 : 0, result.detail.c_str());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Boot Keyboard Peer";
  Serial.printf("DEVICE_PRE_BEGIN heap=%u\n", static_cast<unsigned>(ESP.getFreeHeap()));
  if (!ble.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.printf("DEVICE_POST_BEGIN heap=%u\n", static_cast<unsigned>(ESP.getFreeHeap()));
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("DEVICE_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    Serial.printf("DEVICE_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    ble.advertising().start();
    Serial.printf("DEVICE_READVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
  });
  ble.advertising().setName("EspBle Boot Keyboard Peer");
  ble.advertising().addServiceUuid("1812");
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("DEVICE_ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'k')
    {
      const uint8_t report[8] = {0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
      Serial.printf("DEVICE_NOTIFY_STARTED success=%u\n",
        notifyReport(report, sizeof(report)) ? 1 : 0);
    }
    else if (command == 'r')
    {
      const uint8_t report[8] = {};
      Serial.printf("DEVICE_NOTIFY_STARTED success=%u\n",
        notifyReport(report, sizeof(report)) ? 1 : 0);
    }
    else if (command == 'w')
    {
      // Wrong-length input report: hosts must not interpret it as key state
      // but must count it so "keys stopped arriving" is diagnosable.
      const uint8_t report[5] = {0x02, 0x00, 0x04, 0x00, 0x00};
      Serial.printf("DEVICE_NOTIFY_STARTED success=%u\n",
        notifyReport(report, sizeof(report)) ? 1 : 0);
    }
    else if (command == 'd')
    {
      Serial.printf("DEVICE_DISCONNECT_STARTED success=%u\n", ble.disconnect(connectionId) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
