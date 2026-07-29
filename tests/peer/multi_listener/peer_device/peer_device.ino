// multi_listener peer_device: GATT server whose write event has one primary
// callback (onWritten) plus additional listeners (addWrittenListener). Every
// observer must see the same write, removeListener() must stop exactly one of
// them, and the list must refuse a listener past its capacity.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAE";
static constexpr const char *CHARACTERISTIC_UUID = "2ae3";

EspBle ble;
EspBleGattService service;
EspBleGattCharacteristic characteristic;

String lastValue;
unsigned primaryCount = 0;
unsigned firstCount = 0;
unsigned secondCount = 0;
EspBleListenerId firstListener = EspBleInvalidListenerId;
EspBleListenerId secondListener = EspBleInvalidListenerId;
// How many listeners this sketch believes are registered, so the capacity check
// reports an absolute number instead of one that depends on what ran before.
unsigned activeListeners = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  valueConfig.writable = true;

  auto &gattServer = ble.gattServer();
  service = gattServer.addService(SERVICE_UUID);
  characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);

  gattServer.onWritten([](const EspBleGattWrite &write) {
    lastValue = write.value;
    ++primaryCount;
  });
  firstListener = gattServer.addWrittenListener([](const EspBleGattWrite &) {
    ++firstCount;
  });
  secondListener = gattServer.addWrittenListener([](const EspBleGattWrite &) {
    ++secondCount;
  });
  if (firstListener != EspBleInvalidListenerId) ++activeListeners;
  if (secondListener != EspBleInvalidListenerId) ++activeListeners;

  EspBleConfig config;
  config.deviceName = "EspBle MultiListener Peer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.printf("LISTENERS_REGISTERED first=%u second=%u\n",
    static_cast<unsigned>(firstListener), static_cast<unsigned>(secondListener));

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle MultiListener Peer");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();
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
    else if (command == 's')
    {
      Serial.printf("WRITE_STATE value=%s primary=%u first=%u second=%u\n",
        lastValue.c_str(), primaryCount, firstCount, secondCount);
    }
    else if (command == 'z')
    {
      primaryCount = 0;
      firstCount = 0;
      secondCount = 0;
      lastValue = "";
      Serial.println("WRITE_STATE_RESET");
    }
    else if (command == 'r')
    {
      // Remove exactly one listener; the primary and the other one stay.
      const bool removed = ble.gattServer().removeListener(firstListener);
      Serial.printf("LISTENER_REMOVED success=%u id=%u\n",
        removed ? 1 : 0, static_cast<unsigned>(firstListener));
      if (removed) --activeListeners;
      firstListener = EspBleInvalidListenerId;
    }
    else if (command == 'u')
    {
      // Removing an id that is not registered must report failure, not succeed
      // silently or take out someone else's listener.
      Serial.printf("LISTENER_REMOVE_UNKNOWN success=%u\n",
        ble.gattServer().removeListener(9999) ? 1 : 0);
    }
    else if (command == 'F')
    {
      // Fill the list to capacity and confirm the next add is refused rather
      // than dropping an existing observer.
      unsigned added = 0;
      while (true)
      {
        const EspBleListenerId id =
          ble.gattServer().addWrittenListener([](const EspBleGattWrite &) {});
        if (id == EspBleInvalidListenerId) break;
        ++added;
        ++activeListeners;
        if (added > 16) break; // safety net; the list must refuse long before this
      }
      Serial.printf("LISTENERS_FILLED added=%u total=%u\n", added, activeListeners);
    }
  }

  ble.update();
  delay(1);
}
