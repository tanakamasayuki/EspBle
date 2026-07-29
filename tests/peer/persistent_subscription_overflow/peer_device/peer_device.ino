// persistent_subscription_overflow peer_device: a Peripheral with 12 notifiable
// characteristics (the server's CCCD tracking capacity) that can change its own
// address type at runtime.
//
// The central records one persistent subscription per (peer address, service,
// characteristic). Filling the 16-entry registry from one address is impossible:
// the central's active subscription table is also 16 and fills first, and a
// subscribe that fails there never reaches the CCCD write, so no record is made.
// Reconnecting to the SAME address does not help either, because the records are
// restored automatically and occupy the active table again.
//
// So this sketch re-inits with a different own address type on command. The
// central then sees a different peer, restores nothing, and its records keep
// accumulating past 16.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "2f9b0000-3a71-4d1e-9c3f-8a5d6e7f1000";
static const char *CHARACTERISTIC_UUIDS[] = {
  "2f9b0001-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0002-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0003-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0004-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0005-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0006-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0007-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0008-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0009-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b000a-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b000b-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b000c-3a71-4d1e-9c3f-8a5d6e7f1000",
};
static constexpr size_t CharacteristicCount =
  sizeof(CHARACTERISTIC_UUIDS) / sizeof(CHARACTERISTIC_UUIDS[0]);

EspBle ble;
EspBleGattCharacteristic characteristics[CharacteristicCount];
bool randomStaticAddress = false;

static bool buildProfile()
{
  auto &server = ble.gattServer();
  const EspBleGattService service = server.addService(SERVICE_UUID);
  if (!service.valid()) return false;
  for (size_t index = 0; index < CharacteristicCount; ++index)
  {
    EspBleGattCharacteristicConfig config;
    config.readable = true;
    config.notifiable = true;
    characteristics[index] =
      server.addCharacteristic(service, CHARACTERISTIC_UUIDS[index], config);
    if (!characteristics[index].valid()) return false;
    server.setValue(characteristics[index], String(index));
  }
  return true;
}

static bool startStack()
{
  EspBleConfig config;
  config.deviceName = "EspBle Persist Overflow";
  config.ownAddressType = randomStaticAddress
    ? EspBleOwnAddressType::RandomStatic
    : EspBleOwnAddressType::Public;
  if (!ble.begin(config)) return false;
  ble.advertising().setName("EspBle Persist Overflow");
  ble.advertising().addServiceUuid(SERVICE_UUID);
  return ble.advertising().start();
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  if (!buildProfile())
  {
    Serial.printf("PROFILE_FAILED %s %s\n",
      ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_CONNECTED id=%u\n",
      static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u\n",
      static_cast<unsigned>(connection.id));
  });
  if (!startStack())
  {
    Serial.printf("INIT_FAILED %s %s\n",
      ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("PERIPHERAL_READY advertising=%u chars=%u address=%s\n",
        ble.advertising().isAdvertising() ? 1 : 0,
        static_cast<unsigned>(CharacteristicCount),
        ble.localAddress().c_str());
    }
    else if (command == 'R')
    {
      // Re-init as a random static address. end() keeps the service and
      // characteristic definitions and only drops their handles, so begin()
      // registers the same profile again under the new address.
      randomStaticAddress = true;
      ble.end();
      const bool restarted = startStack();
      Serial.printf("PERIPHERAL_READDRESSED success=%u address=%s\n",
        restarted ? 1 : 0, ble.localAddress().c_str());
    }
  }
  ble.update();
  delay(1);
}
