// en: Motion, clicks, wheel and drag
// ja: 移動・クリック・wheel・ドラッグ
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Mouse";
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
  bluetooth.hidMouse().configure(hidConfig);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Mouse";
  // en: Peripheral major class with the pointing-device minor value, so a Host
  //     lists this as a mouse rather than as an uncategorised device.
  // ja: Peripheral major classにpointing deviceのminor値を組み合わせる。Hostが
  //     未分類ではなくmouseとして扱うため。
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x20;
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

  Serial.println("Pair from the Host, then send 'm' 'c' 'w' 'p' 'r'.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'm')
    {
      // en: Motion is relative and signed: the Host adds it to where the
      //     pointer already is.
      // ja: 移動量は相対値かつ符号付きで、Hostが現在位置へ加算する。
      bluetooth.hidMouse().move(12, -8);
    }
    else if (command == 'c')
    {
      // en: One click is a press and a release, which click() sends as two
      //     reports.
      // ja: 1クリックは押下と解放で、click()は2 reportとして送る。
      bluetooth.hidMouse().click(ESP_BLE_HID_MOUSE_LEFT);
    }
    else if (command == 'w')
    {
      // en: The wheel moves without moving the pointer, and held buttons stay
      //     held.
      // ja: wheelはpointerを動かさず、押しているbuttonも保持したままになる。
      bluetooth.hidMouse().wheel(-1);
    }
    else if (command == 'p')
    {
      // en: press() adds to the held buttons rather than replacing them, so a
      //     drag is press, move, release.
      // ja: press()は押している状態へ追加する（置き換えではない）。ドラッグは
      //     press → move → releaseになる。
      bluetooth.hidMouse().press(ESP_BLE_HID_MOUSE_LEFT);
      bluetooth.hidMouse().move(30, 0);
      Serial.printf("buttons held: 0x%02x\n", bluetooth.hidMouse().buttons());
    }
    else if (command == 'r')
    {
      // en: Send a released state so the host does not keep keys or buttons logically held.
      // ja: 解除状態を送信し、Host側でキーやボタンが押されたままにならないようにします。
      bluetooth.hidMouse().releaseAll();
    }
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  delay(1);
}
