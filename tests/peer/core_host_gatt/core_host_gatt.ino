// DUT for the core-host GATT interoperability test. This side is EspBle on a
// NimBLE host; peer_device/ is the BLE wrapper Arduino-ESP32 ships, which is
// Bluedroid on the original ESP32. Both sides are asserted, and only behaviour
// the specification requires of both is checked.
#include <EspBle.h>

static const char *ServiceUuid = "2f7a1000-9d0b-4f6a-9b41-1c8f3a5d0001";
static const char *DataUuid = "2f7a1001-9d0b-4f6a-9b41-1c8f3a5d0001";
static const char *NotifyUuid = "2f7a1002-9d0b-4f6a-9b41-1c8f3a5d0001";
static const char *IndicateUuid = "2f7a1003-9d0b-4f6a-9b41-1c8f3a5d0001";

EspBle ble;
bool connectRequested = false;
EspBleConnectionId connectionId = 0;
String peerAddress;
uint16_t negotiatedMtu = 0;
bool discovered = false;

String toHex(const String &value)
{
  String text;
  char octet[3];
  for (size_t index = 0; index < value.length(); ++index)
  {
    snprintf(octet, sizeof(octet), "%02x", static_cast<uint8_t>(value[index]));
    text += octet;
  }
  return text;
}

void reportState()
{
  Serial.printf("COREGATT_STATE connected=%u discovered=%u mtu=%u peer=%s\n",
    connectionId != 0 ? 1 : 0, discovered ? 1 : 0, negotiatedMtu,
    peerAddress.length() != 0 ? peerAddress.c_str() : "none");
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle CoreHost Central";
  if (!ble.begin(config))
  {
    Serial.printf("COREGATT_INIT_FAILED %s:%s\n", ble.lastErrorName(),
      ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectRequested || !result.advertisesService(ServiceUuid)) return;
    ble.scanner().stop();
    peerAddress = result.address;
    connectRequested = ble.connect(result);
    Serial.printf("COREGATT_CONNECT requested=%u peer=%s\n",
      connectRequested ? 1 : 0, peerAddress.c_str());
  });

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    negotiatedMtu = connection.mtu;
    Serial.printf("COREGATT_CONNECTED id=%u mtu=%u\n", connection.id, connection.mtu);
    Serial.printf("COREGATT_DISCOVER requested=%u\n",
      ble.discoverServices(connection.id) ? 1 : 0);
  });

  ble.onMtuChanged([](const EspBleMtuChanged &event) {
    negotiatedMtu = event.connection.mtu;
    Serial.printf("COREGATT_MTU value=%u previous=%u\n", event.connection.mtu,
      event.previousMtu);
  });

  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    // Count what the Bluedroid server actually published. A stack that hides
    // the CCCD or renumbers handles shows up as a different count here.
    const size_t characteristics =
      ble.discoveredCharacteristicCount(result.connectionId, ServiceUuid);
    const size_t descriptors =
      ble.discoveredDescriptorCount(result.connectionId, ServiceUuid, NotifyUuid);
    discovered = result.success;
    Serial.printf("COREGATT_DISCOVERED success=%u chars=%u notify_descs=%u\n",
      result.success ? 1 : 0, static_cast<unsigned>(characteristics),
      static_cast<unsigned>(descriptors));
  });

  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf("COREGATT_READ success=%u value=%s\n",
      result.success ? 1 : 0, result.value.c_str());
  });

  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    Serial.printf("COREGATT_WROTE success=%u response=%u\n",
      result.success ? 1 : 0, result.response ? 1 : 0);
  });

  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("COREGATT_SUBSCRIBED success=%u uuid=%s\n",
      result.success ? 1 : 0, result.characteristicUuid.c_str());
  });

  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf("COREGATT_NOTIFY uuid=%s length=%u hex=%s indication=%u\n",
      notification.characteristicUuid.c_str(),
      static_cast<unsigned>(notification.value.length()),
      toHex(notification.value).c_str(),
      notification.indication ? 1 : 0);
  });

  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    discovered = false;
    negotiatedMtu = 0;
    Serial.printf("COREGATT_DISCONNECTED reason=%u\n", connection.disconnectReason);
  });

  Serial.println("COREGATT_READY");
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "?")
    {
      Serial.println("COREGATT_READY");
      reportState();
    }
    else if (command == "s")
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.printf("COREGATT_SCAN started=%u\n",
        ble.scanner().start(scanConfig) ? 1 : 0);
    }
    else if (command == "r" && connectionId != 0)
    {
      Serial.printf("COREGATT_READ_REQUESTED %u\n",
        ble.readCharacteristic(connectionId, ServiceUuid, DataUuid) ? 1 : 0);
    }
    else if (command == "w" && connectionId != 0)
    {
      // Contains 0x00 so a peer that treats the value as a C string truncates.
      const uint8_t payload[4] = {0x11, 0x00, 0x22, 0x33};
      Serial.printf("COREGATT_WRITE_REQUESTED %u\n",
        ble.writeCharacteristic(connectionId, ServiceUuid, DataUuid, payload,
          sizeof(payload), true)
          ? 1
          : 0);
    }
    else if (command == "n" && connectionId != 0)
    {
      Serial.printf("COREGATT_SUBSCRIBE_REQUESTED %u\n",
        ble.subscribe(connectionId, ServiceUuid, NotifyUuid) ? 1 : 0);
    }
    else if (command == "i" && connectionId != 0)
    {
      Serial.printf("COREGATT_INDICATE_SUBSCRIBE_REQUESTED %u\n",
        ble.subscribe(connectionId, ServiceUuid, IndicateUuid, false) ? 1 : 0);
    }
    else if (command == "u" && connectionId != 0)
    {
      Serial.printf("COREGATT_UNSUBSCRIBE_REQUESTED %u\n",
        ble.unsubscribe(connectionId, ServiceUuid, NotifyUuid) ? 1 : 0);
    }
    else if (command == "x" && connectionId != 0)
    {
      Serial.printf("COREGATT_DISCONNECT_REQUESTED %u\n",
        ble.disconnect(connectionId) ? 1 : 0);
    }
    else if (command == "a")
    {
      connectRequested = false;
      Serial.println("COREGATT_REARMED");
    }
  }

  ble.update();
  delay(1);
}
