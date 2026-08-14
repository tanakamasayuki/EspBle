// en: A Classic HID keyboard and mouse. The calls are the same ones the BLE
//     examples use (examples/Hid/KeyboardDevice), because the reports and the
//     descriptors are shared; only the radio differs.
// ja: ClassicのHID keyboard / mouse。BLE example（examples/Hid/KeyboardDevice）と
//     同じ呼び出しになる。reportとdescriptorを共有しており、違うのは無線だけ。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  // en: Profiles are configured before begin(): the composed Report Descriptor
  //     is part of the device record the Host stores when it pairs.
  // ja: profileはbegin()より前に設定する。合成したReport Descriptorは、Hostが
  //     pairing時に保存するdevice recordの一部になるため。
  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Keyboard";
  bluetooth.hidKeyboard().configure(hidConfig);
  bluetooth.hidMouse().configure(hidConfig);

  bluetooth.hidKeyboard().onOutputReport(
    [](const EspBleClassicHidKeyboardLeds &leds) {
      Serial.printf("LEDs from %s: caps=%u num=%u\n",
        leds.peerAddress.c_str(), leds.capsLock ? 1 : 0, leds.numLock ? 1 : 0);
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Keyboard";
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  Serial.println("Pair from the host, then press the button on GPIO 0");
  pinMode(0, INPUT_PULLUP);
}

void loop()
{
  bluetooth.update();

  static bool pressed;
  const bool down = digitalRead(0) == LOW;
  if (down && !pressed && bluetooth.hidKeyboard().ready())
  {
    // en: Layout-aware text entry, the same call as on BLE.
    // ja: layoutを考慮した文字入力。BLEと同じ呼び出し。
    bluetooth.hidKeyboard().write("hello");
    bluetooth.hidMouse().move(10, 0);
  }
  pressed = down;
  delay(10);
}
