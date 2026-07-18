#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId keyboardConnectionId = 0;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  auto &keyboard = ble.hidHost();
  keyboard.onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf(
      "HOST_DISCOVERED success=%u report=%u country_present=%u country=%u output=%u battery_present=%u battery=%u context=%s detail=%s\n",
      result.success ? 1 : 0,
      result.reportId,
      result.hasCountryCode ? 1 : 0,
      result.countryCode,
      result.hasOutputReport ? 1 : 0,
      result.hasBatteryLevel ? 1 : 0,
      result.batteryLevel,
      callbackContext(),
      result.detail.c_str());
  });
  keyboard.onKeyboardState([](const EspBleHidKeyboardState &state) {
    Serial.printf(
      "HOST_STATE id=%u modifiers=%u a_down=%u a_pressed=%u a_released=%u context=%s\n",
      static_cast<unsigned>(state.connectionId),
      state.modifiers,
      state.isDown(0x04) ? 1 : 0,
      state.wasPressed(0x04) ? 1 : 0,
      state.wasReleased(0x04) ? 1 : 0,
      callbackContext());
  });
  keyboard.onKeyboard([](const EspBleHidKeyboardEvent &event) {
    Serial.printf(
      "HOST_KEY usage=%u ascii=%u pressed=%u released=%u modifiers=%u context=%s\n",
      event.usage,
      event.ascii,
      event.pressed ? 1 : 0,
      event.released ? 1 : 0,
      event.modifiers,
      callbackContext());
  });
  keyboard.onMouse([](const EspBleHidMouseEvent &event) {
    Serial.printf("HOST_MOUSE x=%d y=%d wheel=%d buttons=%u moved=%u changed=%u context=%s\n",
      event.x, event.y, event.wheel, event.buttons, event.moved ? 1 : 0,
      event.buttonsChanged ? 1 : 0, callbackContext());
  });
  keyboard.onConsumerControl([](const EspBleHidConsumerControlEvent &event) {
    Serial.printf("HOST_CONSUMER usage=%u pressed=%u released=%u context=%s\n",
      event.usage, event.pressed ? 1 : 0, event.released ? 1 : 0, callbackContext());
  });
  keyboard.onSystemControl([](const EspBleHidSystemControlEvent &event) {
    Serial.printf("HOST_SYSTEM usage=%u pressed=%u released=%u context=%s\n",
      event.usage, event.pressed ? 1 : 0, event.released ? 1 : 0, callbackContext());
  });
  keyboard.onGamepad([](const EspBleHidGamepadEvent &event) {
    Serial.printf("HOST_GAMEPAD fields=%u changed=%u x=%ld context=%s\n",
      static_cast<unsigned>(event.fieldCount), event.changed ? 1 : 0,
      event.fieldCount > 0 ? static_cast<long>(event.fields[0].value) : 0L, callbackContext());
  });

  EspBleConfig config;
  config.deviceName = "EspBle HID Host Test";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("HOST_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    keyboardConnectionId = connection.id;
    Serial.printf("HOST_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "HOST_SECURITY encrypted=%u bonded=%u context=%s\n",
      event.connection.encrypted ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      callbackContext());
    if (event.success)
    {
      Serial.printf(
        "HOST_DISCOVERY_STARTED success=%u\n",
        ble.hidHost().discover(event.connection.id) ? 1 : 0);
    }
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("HOST_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), callbackContext());
    keyboardConnectionId = 0;
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
    else if (command == 'l')
    {
      Serial.printf(
        "HOST_LEDS_WRITTEN success=%u\n",
        ble.hidHost().setKeyboardLeds(
          keyboardConnectionId, true, true, false) ? 1 : 0);
    }
    else if (command == 'L')
    {
      unsigned success = 0;
      const uint32_t startMs = millis();
      for (int i = 0; i < 10; ++i)
      {
        if (ble.hidHost().setKeyboardLeds(
              keyboardConnectionId, true, true, false))
        {
          ++success;
        }
        delay(5);
      }
      Serial.printf(
        "HOST_LEDS_TIMED success=%u ms=%u\n",
        success,
        static_cast<unsigned>(millis() - startMs));
    }
    else if (command == 'e')
    {
      ble.hidHost().setKeyboardLayout(EspBleKeyboardLayout::EnUs);
      Serial.println("HOST_LAYOUT en-US");
    }
    else if (command == 'j')
    {
      ble.hidHost().setKeyboardLayout(EspBleKeyboardLayout::JaJp);
      Serial.println("HOST_LAYOUT ja-JP");
    }
    else if (command == 'g')
    {
      ble.hidHost().setKeyboardLayout(EspBleKeyboardLayout::DeDe);
      Serial.println("HOST_LAYOUT de-DE");
    }
    else if (command == 'f')
    {
      ble.hidHost().setKeyboardLayout(EspBleKeyboardLayout::FrFr);
      Serial.println("HOST_LAYOUT fr-FR");
    }
    else if (command == 'b')
    {
      ble.hidHost().setKeyboardLayout(EspBleKeyboardLayout::EnGb);
      Serial.println("HOST_LAYOUT en-GB");
    }
    else if (command == 'd')
    {
      Serial.printf("HOST_DISCONNECT_STARTED success=%u\n",
        ble.disconnect(keyboardConnectionId) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
