// en: Keyboard, mouse and media keys in one device, and the SDP record limit on how many fit
// ja: keyboard・mouse・メディアキーを1台で兼ねる。合成数のSDP record上限も示す
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
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
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
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
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
    // en: Mouse coordinates here are relative deltas, not absolute screen positions.
    // ja: ここでのマウス座標は相対移動量で、画面上の絶対位置ではありません。
    else if (command == 'm') bluetooth.hidMouse().move(20, 0);
    else if (command == 'v')
      bluetooth.hidConsumerControl().click(
        ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP);
    else if (command == 'r')
    {
      // en: Send a released state so the host does not keep keys or buttons logically held.
      // ja: 解除状態を送信し、Host側でキーやボタンが押されたままにならないようにします。
      bluetooth.hidKeyboard().releaseAll();
      bluetooth.hidMouse().releaseAll();
    }
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  delay(1);
}
