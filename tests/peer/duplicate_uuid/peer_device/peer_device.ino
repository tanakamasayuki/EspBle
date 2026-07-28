// Peripheral for the duplicate_uuid peer test: register two Services that share
// a UUID (allowed), and attempt a second Characteristic with a UUID already used
// in the same Service (rejected by the bundled backend, so EspBle refuses it).
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-647570736572";
static constexpr const char *VALUE_UUID = "5266f728-49d7-4eaf-a6f1-647570636861";

EspBle ble;
EspBleGattCharacteristic firstValue;
EspBleGattCharacteristic duplicateValue;
EspBleGattCharacteristic otherServiceValue;

void setup()
{
  Serial.begin(115200);
  delay(500);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig config;
  config.readable = true;
  config.notifiable = true;

  // Two instances of the same Service UUID: each call yields its own handle.
  const EspBleGattService first = gattServer.addService(SERVICE_UUID);
  const EspBleGattService second = gattServer.addService(SERVICE_UUID);

  firstValue = gattServer.addCharacteristic(first, VALUE_UUID, config);
  // Same UUID in the same service: must be refused, not silently merged.
  duplicateValue = gattServer.addCharacteristic(first, VALUE_UUID, config);
  // Same UUID in a different service: fine.
  otherServiceValue = gattServer.addCharacteristic(second, VALUE_UUID, config);

  gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf(
      "SUBSCRIPTION id=%u notify=%u\n",
      subscription.characteristic.id, subscription.notifications ? 1 : 0);
  });

  gattServer.setValue(firstValue, String("first"));
  gattServer.setValue(otherServiceValue, String("other"));

  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle Duplicate UUID";
  if (!ble.begin(bleConfig))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.advertising().setName("EspBle Duplicate UUID");
  ble.advertising().addServiceUuid(SERVICE_UUID);
  if (!ble.advertising().start())
  {
    Serial.printf("ADVERTISING_FAILED %s\n", ble.lastErrorDetail().c_str());
  }
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
    // Notify one instance at a time: a send is asynchronous, so the central can
    // only attribute a notification to a characteristic if they do not overlap.
    else if (command == '1')
    {
      Serial.printf(
        "NOTIFIED first=%u\n",
        ble.gattServer().notify(firstValue, String("ping-first")) ? 1 : 0);
    }
    else if (command == '2')
    {
      Serial.printf(
        "NOTIFIED other=%u\n",
        ble.gattServer().notify(otherServiceValue, String("ping-other")) ? 1 : 0);
    }
    else if (command == 'h')
    {
      Serial.printf(
        "HANDLES first=%u duplicate=%u other=%u\n",
        firstValue.id, duplicateValue.id, otherServiceValue.id);
    }
  }
  ble.update();
  delay(1);
}
