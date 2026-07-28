// Peripheral for the duplicate_uuid peer test: expose two Services that share a
// UUID, and two Characteristics that share a UUID inside one of them. This is
// what the spec allows and what a UUID-keyed API cannot express.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-647570736572";
static constexpr const char *VALUE_UUID = "5266f728-49d7-4eaf-a6f1-647570636861";

EspBle ble;
EspBleGattCharacteristic firstValue;
EspBleGattCharacteristic secondValue;
EspBleGattCharacteristic otherServiceValue;

void setup()
{
  Serial.begin(115200);
  delay(500);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig config;
  config.readable = true;
  config.notifiable = true;

  // Two instances of the same Service UUID.
  const EspBleGattService first = gattServer.addService(SERVICE_UUID);
  const EspBleGattService second = gattServer.addService(SERVICE_UUID);

  // Two Characteristics sharing a UUID inside the first Service, plus one more
  // with the same UUID in the second Service.
  firstValue = gattServer.addCharacteristic(first, VALUE_UUID, config);
  secondValue = gattServer.addCharacteristic(first, VALUE_UUID, config);
  otherServiceValue = gattServer.addCharacteristic(second, VALUE_UUID, config);

  // Distinct values prove the handles address distinct attributes.
  gattServer.setValue(firstValue, String("first"));
  gattServer.setValue(secondValue, String("second"));
  gattServer.setValue(otherServiceValue, String("other"));

  EspBleConfig config2;
  config2.deviceName = "EspBle Duplicate UUID";
  if (!ble.begin(config2))
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
    else if (command == 'h')
    {
      Serial.printf(
        "HANDLES first=%u second=%u other=%u\n",
        firstValue.id, secondValue.id, otherServiceValue.id);
    }
  }
  ble.update();
  delay(1);
}
