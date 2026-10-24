// en: N-key rollover, with no six-key limit
// ja: N-key rollover。6キー制限が無い
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
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
  bluetooth.hidKeyboard().configure(hidConfig);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic NKRO";
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x10;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
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
      // en: Send a released state so the host does not keep keys or buttons logically held.
      // ja: 解除状態を送信し、Host側でキーやボタンが押されたままにならないようにします。
      bluetooth.hidKeyboard().releaseAll();
    }
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  delay(1);
}
