// en: A Bluetooth Classic (BR/EDR) HID mouse. The calls are the same ones the
//     BLE example uses (examples/Hid/Mouse); only the radio differs.
// ja: Bluetooth Classic（BR/EDR）のHID mouse。呼び出しはBLE example
//     （examples/Hid/Mouse）と同じで、違うのは無線だけ。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Mouse";
  bluetooth.hidMouse().configure(hidConfig);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Mouse";
  // en: Peripheral major class with the pointing-device minor value, so a Host
  //     lists this as a mouse rather than as an uncategorised device.
  // ja: Peripheral major classにpointing deviceのminor値を組み合わせる。Hostが
  //     未分類ではなくmouseとして扱うため。
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x20;
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
      bluetooth.hidMouse().releaseAll();
    }
  }

  bluetooth.update();
  delay(1);
}
