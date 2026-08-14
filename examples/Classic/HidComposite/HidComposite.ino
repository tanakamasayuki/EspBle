// en: One Bluetooth Classic (BR/EDR) HID device that is a keyboard, a mouse and
//     media keys at once. Classic registers a single device record, so all of it
//     goes into one composed Report Descriptor and each profile keeps its own
//     report ID. Mirrors examples/Hid/CompositeKeyboardMouse on the BLE side.
//
//     How many profiles fit is limited here in a way it is not on BLE: the
//     record has to fit a 300-byte SDP pad shared with the device strings, and
//     these three come to 144 descriptor bytes. Adding the gamepad makes 212,
//     which does not register — see examples/Classic/HidGamepad for that one on
//     its own.
// ja: keyboard、mouse、メディアキーを兼ねる1台のBluetooth Classic（BR/EDR）
//     HID device。Classicはdevice recordを1つ登録するため、すべてが1つの合成
//     Report Descriptorに入り、profileごとにreport IDが分かれる。
//     BLE側のexamples/Hid/CompositeKeyboardMouseと対になる。
//
//     合成できるprofile数にはBLEには無い上限がある。recordはdevice名などと共有する
//     300 byteのSDP padに収まる必要があり、この3つでdescriptorは144 byteになる。
//     gamepadを加えると212 byteで登録できない——単独の例は
//     examples/Classic/HidGamepadにある。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  // en: Every profile is configured before begin(). What is configured decides
  //     the descriptor, so a profile added later would not be in the record the
  //     Host already read — and begin() refuses a combination that would not fit
  //     the SDP record rather than starting a device no Host can reach.
  // ja: profileはすべてbegin()より前に設定する。設定した内容がdescriptorを決めるため、
  //     後から追加してもHostが読み終えたrecordには入らない。SDP recordに収まらない
  //     組み合わせはbegin()が拒否する——誰も到達できないdeviceを起動しないため。
  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Composite";
  bluetooth.hidKeyboard().configure(hidConfig);
  bluetooth.hidMouse().configure(hidConfig);
  bluetooth.hidConsumerControl().configure(hidConfig);

  bluetooth.hidKeyboard().onOutputReport(
    [](const EspBleClassicHidKeyboardLeds &leds) {
      Serial.printf("LEDs: caps=%u num=%u\n",
        leds.capsLock ? 1 : 0, leds.numLock ? 1 : 0);
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Composite";
  // en: A composite device has to pick one class. Keyboard is the honest choice
  //     when it types, since that is what a Host will offer it as.
  // ja: 複合deviceでもclassは1つ選ぶ。文字入力をするならkeyboardが妥当で、Hostは
  //     その姿で提示する。
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x10;
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.hidDevice().onConnected(
    [](const EspBleClassicHidConnection &connection) {
      Serial.printf("Host connected: %s\n", connection.peerAddress.c_str());
    });

  Serial.println("Pair from the Host, then send 'k' 'm' 'v' 'r'.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    // en: The profiles share one device but not one report: each call goes out
    //     under its own report ID, so a Host that only understands the keyboard
    //     ignores the rest.
    // ja: profileはdeviceを共有するがreportは共有しない。呼び出しはそれぞれの
    //     report IDで出るため、keyboardしか解釈しないHostは他を無視する。
    if (command == 'k') bluetooth.hidKeyboard().write("hi");
    else if (command == 'm') bluetooth.hidMouse().move(20, 0);
    else if (command == 'v')
      bluetooth.hidConsumerControl().click(
        ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP);
    else if (command == 'r')
    {
      bluetooth.hidKeyboard().releaseAll();
      bluetooth.hidMouse().releaseAll();
    }
  }

  bluetooth.update();
  delay(1);
}
