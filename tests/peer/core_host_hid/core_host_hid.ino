// DUT for the core-host HID interoperability test. EspBle's HID Host runs on a
// NimBLE host here, while peer_device/ is a HOGP keyboard built with
// BLEHIDDevice on Bluedroid. The Report Map, the report characteristics and the
// LED output report are therefore produced by a different stack than the one
// parsing them.
#include <EspBle.h>

EspBle ble;
bool connectRequested = false;
EspBleConnectionId connectionId = 0;
bool discoveryDone = false;
unsigned keyEvents = 0;

void reportState()
{
  Serial.printf("HIDHOST_STATE connected=%u discovered=%u key_events=%u\n",
    connectionId != 0 ? 1 : 0, discoveryDone ? 1 : 0, keyEvents);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle CoreHost HID Host";
  // HOGP keeps its Report Map behind encryption, so the host has to pair before
  // it can discover anything.
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.pairOnConnect = true;
  config.security.ioCapability = EspBleSecurityIoCapability::None;
  if (!ble.begin(config))
  {
    Serial.printf("HIDHOST_INIT_FAILED %s:%s\n", ble.lastErrorName(),
      ble.lastErrorDetail().c_str());
    return;
  }

  ble.hidHost().onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    discoveryDone = result.success;
    // report_id=0 is what a device without report IDs must produce; a parser
    // that invents one would show a different value here.
    Serial.printf(
      "HIDHOST_DISCOVERED success=%u report_id=%u output=%u battery=%u level=%u\n",
      result.success ? 1 : 0, result.reportId, result.hasOutputReport ? 1 : 0,
      result.hasBatteryLevel ? 1 : 0, result.batteryLevel);
  });

  ble.hidHost().onKeyboard([](const EspBleHidKeyboardEvent &event) {
    if (event.pressed) ++keyEvents;
    Serial.printf("HIDHOST_KEY usage=%02x ascii=%c modifiers=%02x pressed=%u\n",
      event.usage, event.ascii != 0 ? event.ascii : '.', event.modifiers,
      event.pressed ? 1 : 0);
  });

  ble.hidHost().onKeyboardState([](const EspBleHidKeyboardState &state) {
    Serial.printf("HIDHOST_STATE_REPORT modifiers=%02x a=%u numlock=%u\n",
      state.modifiers, state.isDown(0x04) ? 1 : 0, state.numLock ? 1 : 0);
  });

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("HIDHOST_CONNECTED id=%u\n", connection.id);
  });

  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf("HIDHOST_SECURITY success=%u encrypted=%u bonded=%u\n",
      event.success ? 1 : 0, event.connection.encrypted ? 1 : 0,
      event.connection.bonded ? 1 : 0);
    if (event.success && !discoveryDone)
    {
      Serial.printf("HIDHOST_DISCOVER_REQUESTED %u\n",
        ble.hidHost().discover(event.connection.id) ? 1 : 0);
    }
  });

  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    discoveryDone = false;
    Serial.printf("HIDHOST_DISCONNECTED reason=%u\n", connection.disconnectReason);
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectRequested || !result.connectable || !result.advertisesService("1812"))
    {
      return;
    }
    ble.scanner().stop();
    connectRequested = ble.connect(result);
    Serial.printf("HIDHOST_CONNECT requested=%u peer=%s\n",
      connectRequested ? 1 : 0, result.address.c_str());
  });

  Serial.println("HIDHOST_READY");
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "?")
    {
      Serial.println("HIDHOST_READY");
      reportState();
    }
    else if (command == "s")
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.printf("HIDHOST_SCAN started=%u\n",
        ble.scanner().start(scanConfig) ? 1 : 0);
    }
    else if (command == "l" && connectionId != 0)
    {
      Serial.printf("HIDHOST_LED_REQUESTED %u\n",
        ble.hidHost().setKeyboardLeds(connectionId, true, false, false) ? 1 : 0);
    }
    else if (command == "L" && connectionId != 0)
    {
      Serial.printf("HIDHOST_LED_REQUESTED %u\n",
        ble.hidHost().setKeyboardLeds(connectionId, false, false, false) ? 1 : 0);
    }
    else if (command == "x" && connectionId != 0)
    {
      Serial.printf("HIDHOST_DISCONNECT_REQUESTED %u\n",
        ble.disconnect(connectionId) ? 1 : 0);
    }
    else if (command == "a")
    {
      connectRequested = false;
      Serial.println("HIDHOST_REARMED");
    }
  }

  ble.update();
  delay(1);
}
