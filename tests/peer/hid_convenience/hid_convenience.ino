// hid_convenience DUT: the HID Host side. It receives what the peer sends
// through the convenience input APIs (pressKey/tapKey/write/wheel/click/...) and
// prints every event so the test can check the reports those APIs produce.
// It also exercises EspBleHidHost's multi-listener API (addKeyboardListener /
// addMouseListener / removeListener) on top of the primary onKeyboard/onMouse.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId deviceConnectionId = 0;
EspBleListenerId keyListener1 = 0;
EspBleListenerId keyListener2 = 0;
EspBleListenerId mouseListener1 = 0;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  auto &host = ble.hidHost();
  host.onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf(
      "HOST_DISCOVERED success=%u report=%u country=%u battery=%u context=%s detail=%s\n",
      result.success ? 1 : 0,
      result.reportId,
      result.hasCountryCode ? result.countryCode : 0,
      result.hasBatteryLevel ? result.batteryLevel : 0,
      callbackContext(),
      result.detail.c_str());
  });
  host.onKeyboard([](const EspBleHidKeyboardEvent &event) {
    Serial.printf("HOST_KEY usage=%u ascii=%u pressed=%u released=%u modifiers=%u\n",
      event.usage, event.ascii, event.pressed ? 1 : 0, event.released ? 1 : 0,
      event.modifiers);
  });
  host.onMouse([](const EspBleHidMouseEvent &event) {
    Serial.printf("HOST_MOUSE x=%d y=%d wheel=%d buttons=%u moved=%u changed=%u\n",
      event.x, event.y, event.wheel, event.buttons, event.moved ? 1 : 0,
      event.buttonsChanged ? 1 : 0);
  });
  host.onConsumerControl([](const EspBleHidConsumerControlEvent &event) {
    Serial.printf("HOST_CONSUMER usage=%u pressed=%u released=%u\n",
      event.usage, event.pressed ? 1 : 0, event.released ? 1 : 0);
  });
  host.onSystemControl([](const EspBleHidSystemControlEvent &event) {
    Serial.printf("HOST_SYSTEM usage=%u pressed=%u released=%u\n",
      event.usage, event.pressed ? 1 : 0, event.released ? 1 : 0);
  });
  host.onGamepad([](const EspBleHidGamepadEvent &event) {
    Serial.printf("HOST_GAMEPAD fields=%u changed=%u x=%ld hat=%ld\n",
      static_cast<unsigned>(event.fieldCount), event.changed ? 1 : 0,
      event.fieldCount > 0 ? static_cast<long>(event.fields[0].value) : 0L,
      event.fieldCount > 6 ? static_cast<long>(event.fields[6].value) : 0L);
  });

  EspBleConfig config;
  config.deviceName = "EspBle HID Convenience Host";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("HOST_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    deviceConnectionId = connection.id;
    Serial.printf("HOST_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf("HOST_SECURITY encrypted=%u bonded=%u\n",
      event.connection.encrypted ? 1 : 0, event.connection.bonded ? 1 : 0);
    if (event.success)
    {
      Serial.printf("HOST_DISCOVERY_STARTED success=%u\n",
        ble.hidHost().discover(event.connection.id) ? 1 : 0);
    }
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("HOST_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), callbackContext());
    deviceConnectionId = 0;
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService("1812"))
    {
      ble.scanner().stop();
      Serial.printf("HOST_CONNECT_STARTED success=%u\n", ble.connect(result) ? 1 : 0);
    }
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("HOST_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 's')
    {
      Serial.printf("HOST_SCAN_STARTED success=%u\n", ble.scanner().start() ? 1 : 0);
    }
    else if (command == 'A')
    {
      keyListener1 = ble.hidHost().addKeyboardListener(
        [](const EspBleHidKeyboardEvent &event) {
          Serial.printf("HOST_KEY_L1 usage=%u pressed=%u\n",
            event.usage, event.pressed ? 1 : 0);
        });
      keyListener2 = ble.hidHost().addKeyboardListener(
        [](const EspBleHidKeyboardEvent &event) {
          Serial.printf("HOST_KEY_L2 usage=%u pressed=%u\n",
            event.usage, event.pressed ? 1 : 0);
        });
      mouseListener1 = ble.hidHost().addMouseListener(
        [](const EspBleHidMouseEvent &event) {
          Serial.printf("HOST_MOUSE_L1 wheel=%d buttons=%u\n",
            event.wheel, event.buttons);
        });
      Serial.printf("HOST_LISTENERS_ADDED key1=%u key2=%u mouse1=%u\n",
        static_cast<unsigned>(keyListener1 != 0 ? 1 : 0),
        static_cast<unsigned>(keyListener2 != 0 ? 1 : 0),
        static_cast<unsigned>(mouseListener1 != 0 ? 1 : 0));
    }
    else if (command == 'R')
    {
      // Removing the second keyboard listener must leave the first one and the
      // primary onKeyboard callback untouched.
      Serial.printf("HOST_LISTENER_REMOVED success=%u\n",
        ble.hidHost().removeListener(keyListener2) ? 1 : 0);
    }
    else if (command == 'r')
    {
      // The same id a second time: already gone, so the call must fail.
      Serial.printf("HOST_LISTENER_REMOVED_AGAIN success=%u\n",
        ble.hidHost().removeListener(keyListener2) ? 1 : 0);
    }
    else if (command == 'C')
    {
      // Per-event listener capacity. keyListener1 is still registered and
      // keyListener2 was removed, so one slot is taken here.
      size_t added = 1;
      while (added < EspBleHidHost::MaxListenersPerEvent + 4)
      {
        const EspBleListenerId id = ble.hidHost().addKeyboardListener(
          [](const EspBleHidKeyboardEvent &) {});
        if (id == 0) break;
        ++added;
      }
      Serial.printf("HOST_LISTENER_CAPACITY total=%u max=%u error=%s\n",
        static_cast<unsigned>(added),
        static_cast<unsigned>(EspBleHidHost::MaxListenersPerEvent),
        ble.lastErrorName());
    }
    else if (command == 'd')
    {
      Serial.printf("HOST_DISCONNECT_STARTED success=%u\n",
        ble.disconnect(deviceConnectionId) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
