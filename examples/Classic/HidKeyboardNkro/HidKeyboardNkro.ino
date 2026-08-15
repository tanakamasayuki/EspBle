// en: A Bluetooth Classic (BR/EDR) keyboard with N-key rollover: every key is a
//     bit, so there is no six-key limit. The calls are the same ones the BLE
//     example uses (examples/Hid/KeyboardNkro).
// ja: N-key rollover対応のBluetooth Classic（BR/EDR）keyboard。各キーが1 bitなので
//     6キー制限が無い。呼び出しはBLE example（examples/Hid/KeyboardNkro）と同じ。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  // en: NKRO changes the Report Descriptor, so it is chosen before configure()
  //     rather than at send time: a Host reads the descriptor once, while
  //     pairing.
  // ja: NKROはReport Descriptorを変えるため、送信時ではなくconfigure()より前に
  //     決める。Hostはpairing時にdescriptorを一度だけ読む。
  bluetooth.hidKeyboard().enableNkro(true);

  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic NKRO";
  bluetooth.hidKeyboard().configure(hidConfig);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic NKRO";
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x10;
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.hidKeyboard().onOutputReport(
    [](const EspBleClassicHidKeyboardLeds &leds) {
      Serial.printf("LEDs: caps=%u num=%u\n",
        leds.capsLock ? 1 : 0, leds.numLock ? 1 : 0);
    });

  Serial.println("Pair from the Host, then send '8' 'w' 'r'.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '8')
    {
      // en: Eight keys at once, which a six-key report cannot express. Each
      //     press() adds to the held state; the whole state travels in one
      //     report.
      // ja: 8キー同時押し。6キーのreportでは表現できない。press()は押下状態へ
      //     追加し、状態全体が1 reportで送られる。
      static const uint8_t usages[] = {
        0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
      for (uint8_t usage : usages) bluetooth.hidKeyboard().pressUsage(usage);
      Serial.println("eight keys held");
    }
    else if (command == 'w')
    {
      // en: Typing works the same as on a 6KRO keyboard: the convenience calls
      //     do not change with the descriptor.
      // ja: 文字入力は6KROと同じ。便利APIはdescriptorが変わっても同じまま。
      bluetooth.hidKeyboard().write("nkro");
    }
    else if (command == 'r')
    {
      bluetooth.hidKeyboard().releaseAll();
    }
  }

  bluetooth.update();
  delay(1);
}
