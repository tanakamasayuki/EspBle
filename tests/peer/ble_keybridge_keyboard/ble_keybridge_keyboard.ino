#include <ESP32KeyBridge.h>
#include <EspBle.h>

#include "EspBleKeyboardInputAdapter.h"

EspBle ble;
EspBleConnectionId keyboardConnectionId = 0;
EspBleKeyboardInputAdapter keyboard(ble);
esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::ManualOutputAdapter output;
esp32keybridge::KeySet lastKeys;

static bool sameKeys(
  const esp32keybridge::KeySet &left,
  const esp32keybridge::KeySet &right)
{
  if (left.count() != right.count())
  {
    return false;
  }
  for (size_t i = 0; i < left.count(); ++i)
  {
    if (!right.contains(left.at(i)))
    {
      return false;
    }
  }
  return true;
}

static void printKeys(const esp32keybridge::KeySet &keys)
{
  esp32keybridge::Key sorted[esp32keybridge::KeySet::MaxKeys];
  for (size_t i = 0; i < keys.count(); ++i)
  {
    sorted[i] = keys.at(i);
  }
  for (size_t i = 1; i < keys.count(); ++i)
  {
    const esp32keybridge::Key key = sorted[i];
    size_t j = i;
    while (j > 0 && sorted[j - 1].code > key.code)
    {
      sorted[j] = sorted[j - 1];
      --j;
    }
    sorted[j] = key;
  }
  Serial.print("BRIDGE_OUT");
  if (keys.count() == 0)
  {
    Serial.print(" empty");
  }
  for (size_t i = 0; i < keys.count(); ++i)
  {
    Serial.printf(" %s:%04x",
      esp32keybridge::keyKindName(sorted[i].kind), sorted[i].code);
  }
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  keyboard.onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf("BRIDGE_DISCOVERED success=%u id=%u\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.connectionId));
  });

  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle KeyBridge Test";
  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;
  if (!ble.begin(bleConfig))
  {
    Serial.printf("BRIDGE_INIT_FAILED %s\n", ble.lastErrorName());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    keyboardConnectionId = connection.id;
    Serial.printf("BRIDGE_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf("BRIDGE_SECURITY success=%u bonded=%u\n",
      event.success ? 1 : 0, event.connection.bonded ? 1 : 0);
    if (event.success)
    {
      Serial.printf("BRIDGE_DISCOVERY_STARTED success=%u\n",
        keyboard.discover(event.connection.id) ? 1 : 0);
    }
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("BRIDGE_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    keyboardConnectionId = 0;
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService("1812"))
    {
      ble.scanner().stop();
      Serial.printf("BRIDGE_CONNECT_STARTED success=%u\n", ble.connect(result) ? 1 : 0);
    }
  });

  bridge.addInput(keyboard);
  bridge.addOutput(output);
  esp32keybridge::ESP32KeyBridgeConfig config;
  config.global.remap(
    esp32keybridge::KeyboardUsage::A,
    esp32keybridge::KeyboardUsage::B);
  bridge.applyConfig(config);
  Serial.println("BRIDGE_READY");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      Serial.printf("BRIDGE_BONDS_CLEARED success=%u count=%u\n",
        ble.deleteAllBonds() ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 's')
    {
      Serial.printf("BRIDGE_SCAN_STARTED success=%u\n", ble.scanner().start() ? 1 : 0);
    }
    else if (command == 'd')
    {
      Serial.printf("BRIDGE_DISCONNECT_STARTED success=%u\n",
        ble.disconnect(keyboardConnectionId) ? 1 : 0);
    }
  }

  bridge.update();
  if (!sameKeys(output.keys(), lastKeys))
  {
    lastKeys = output.keys();
    printKeys(lastKeys);
  }
  delay(1);
}
