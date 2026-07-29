// hid_convenience peer_device: a composite HID device driven only through the
// convenience input APIs. Every command below calls one of them and prints the
// return value; the DUT (HID Host) prints the reports that arrive, so the test
// checks what each convenience call actually puts on the air.
//
// The raw sendReport() paths are covered by hid_keyboard_device /
// hid_keyboard_host; this sketch deliberately never calls them.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspBle ble;
TaskHandle_t loopTask = nullptr;

// Escape and F1. Chosen because neither is reachable through pressKey(): they
// exercise the usage-level APIs rather than the character-level ones.
static constexpr uint8_t UsageEscape = 0x29;
static constexpr uint8_t UsageF1 = 0x3a;

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  auto &keyboard = ble.hidKeyboard();
  EspBleHidKeyboardConfig keyboardConfig;
  keyboardConfig.manufacturer = "EspBle Convenience Peer";
  keyboardConfig.vendorId = 0x303a;
  keyboardConfig.productId = 0x4006;
  keyboardConfig.productVersion = 0x0100;
  keyboardConfig.countryCode = 13;
  keyboardConfig.initialBatteryLevel = 55;
  if (!keyboard.configure(keyboardConfig) || !ble.hidMouse().configure() ||
      !ble.hidConsumerControl().configure() || !ble.hidSystemControl().configure() ||
      !ble.hidGamepad().configure())
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s %s\n",
      ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle HID Convenience";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s %s\n",
      ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("DEVICE_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf("DEVICE_SECURITY encrypted=%u bonded=%u\n",
      event.connection.encrypted ? 1 : 0, event.connection.bonded ? 1 : 0);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("DEVICE_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    // Advertise again so the next test in this file can connect without a reset.
    ble.advertising().start();
  });
  ble.advertising().setName("EspBle HID Convenience");
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    auto &keyboard = ble.hidKeyboard();
    auto &mouse = ble.hidMouse();
    if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("DEVICE_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == '?')
    {
      Serial.printf("DEVICE_ADVERTISING %u\n",
        ble.advertising().isAdvertising() ? 1 : 0);
    }
    // --- keyboard, character level -----------------------------------------
    else if (command == 'a')
    {
      Serial.printf("DEVICE_PRESS_KEY success=%u\n", keyboard.pressKey('a') ? 1 : 0);
    }
    else if (command == 'A')
    {
      // Uppercase needs a modifier the caller never names: pressKey() has to
      // find the Shift combination on its own.
      Serial.printf("DEVICE_PRESS_KEY_SHIFTED success=%u\n",
        keyboard.pressKey('A') ? 1 : 0);
    }
    else if (command == 'r')
    {
      Serial.printf("DEVICE_RELEASE_ALL success=%u\n", keyboard.releaseAll() ? 1 : 0);
    }
    else if (command == 't')
    {
      Serial.printf("DEVICE_TAP_KEY success=%u\n", keyboard.tapKey('b') ? 1 : 0);
    }
    else if (command == 'w')
    {
      Serial.printf("DEVICE_WRITE success=%u\n", keyboard.write("hi") ? 1 : 0);
    }
    else if (command == 'Z')
    {
      // 0x01 is not produced by any key in any layout, so the character cannot
      // be typed and the call must report why.
      Serial.printf("DEVICE_PRESS_KEY_UNMAPPED success=%u error=%s\n",
        keyboard.pressKey('\x01') ? 1 : 0, ble.lastErrorName());
    }
    // --- keyboard, usage level ---------------------------------------------
    else if (command == 'u')
    {
      Serial.printf("DEVICE_TAP_USAGE success=%u\n",
        keyboard.tapUsage(UsageEscape) ? 1 : 0);
    }
    else if (command == 'U')
    {
      Serial.printf("DEVICE_PRESS_USAGE success=%u\n",
        keyboard.pressUsage(UsageF1, EspBleHidKeyboardReport::LeftControl) ? 1 : 0);
    }
    else if (command == 'q')
    {
      // Without NKRO there is no per-key release, so this releases everything.
      Serial.printf("DEVICE_RELEASE_USAGE success=%u\n",
        keyboard.releaseUsage(UsageF1) ? 1 : 0);
    }
    // --- keyboard layout ----------------------------------------------------
    else if (command == 'E')
    {
      keyboard.setLayout(EspBleKeyboardLayout::EnUs);
      Serial.printf("DEVICE_LAYOUT lcid=%u\n",
        static_cast<unsigned>(keyboard.layout()));
    }
    else if (command == 'J')
    {
      keyboard.setLayout(EspBleKeyboardLayout::JaJp);
      Serial.printf("DEVICE_LAYOUT lcid=%u\n",
        static_cast<unsigned>(keyboard.layout()));
    }
    else if (command == 'Q')
    {
      // '"' sits on a different key in en-US (Shift + ') than in ja-JP
      // (Shift + 2), so the usage the host sees shows which layout was used.
      Serial.printf("DEVICE_PRESS_QUOTE success=%u\n", keyboard.pressKey('"') ? 1 : 0);
    }
    // --- mouse --------------------------------------------------------------
    else if (command == 'o')
    {
      Serial.printf("DEVICE_WHEEL success=%u\n", mouse.wheel(3) ? 1 : 0);
    }
    else if (command == 'c')
    {
      Serial.printf("DEVICE_CLICK success=%u\n",
        mouse.click(ESP_BLE_HID_MOUSE_LEFT) ? 1 : 0);
    }
    else if (command == 'P')
    {
      // press() accumulates: the second call must report both buttons.
      const bool first = mouse.press(ESP_BLE_HID_MOUSE_LEFT);
      const bool second = mouse.press(ESP_BLE_HID_MOUSE_RIGHT);
      Serial.printf("DEVICE_PRESS_BUTTONS success=%u buttons=%u\n",
        first && second ? 1 : 0, mouse.buttons());
    }
    else if (command == 'p')
    {
      const bool released = mouse.releaseAll();
      Serial.printf("DEVICE_MOUSE_RELEASE_ALL success=%u buttons=%u\n",
        released ? 1 : 0, mouse.buttons());
    }
    // --- consumer / system control -----------------------------------------
    else if (command == 'k')
    {
      Serial.printf("DEVICE_CONSUMER_USAGE success=%u usage=%u\n",
        ble.hidConsumerControl().sendUsage(ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP) ? 1 : 0,
        ble.hidConsumerControl().usage());
    }
    else if (command == 'K')
    {
      Serial.printf("DEVICE_CONSUMER_CLICK success=%u usage=%u\n",
        ble.hidConsumerControl().click(ESP_BLE_HID_CONSUMER_CONTROL_PLAY_PAUSE) ? 1 : 0,
        ble.hidConsumerControl().usage());
    }
    else if (command == 'y')
    {
      Serial.printf("DEVICE_SYSTEM_USAGE success=%u usage=%u\n",
        ble.hidSystemControl().sendUsage(ESP_BLE_HID_SYSTEM_CONTROL_WAKE_HOST) ? 1 : 0,
        ble.hidSystemControl().usage());
    }
    else if (command == 'Y')
    {
      Serial.printf("DEVICE_SYSTEM_CLICK success=%u usage=%u\n",
        ble.hidSystemControl().click(ESP_BLE_HID_SYSTEM_CONTROL_STANDBY) ? 1 : 0,
        ble.hidSystemControl().usage());
    }
    // --- gamepad ------------------------------------------------------------
    else if (command == 'g')
    {
      Serial.printf("DEVICE_GAMEPAD success=%u\n",
        ble.hidGamepad().send(
          40, -40, 0, 0, 0, 0, ESP_BLE_HID_GAMEPAD_HAT_LEFT, 5) ? 1 : 0);
    }
    else if (command == 'G')
    {
      Serial.printf("DEVICE_GAMEPAD_RELEASE_ALL success=%u\n",
        ble.hidGamepad().releaseAll() ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
