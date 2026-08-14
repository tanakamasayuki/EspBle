// en: A Bluetooth Classic (BR/EDR) HID gamepad. Older game consoles and PCs
//     only accept Classic HID, so BLE is not an alternative for them; the calls
//     are the same ones the BLE example uses (examples/Hid/Gamepad).
// ja: Bluetooth Classic（BR/EDR）のHID gamepad。旧世代のゲーム機やPCはClassic HID
//     しか受け付けないため、BLEでは代替できない。呼び出しはBLE example
//     （examples/Hid/Gamepad）と同じ。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  // en: Profiles are configured before begin(): the composed Report Descriptor
  //     is part of the device record a Host reads when it pairs.
  // ja: profileはbegin()より前に設定する。合成したReport Descriptorは、Hostが
  //     pairing時に読むdevice recordの一部になるため。
  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Gamepad";
  bluetooth.hidGamepad().configure(hidConfig);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Gamepad";
  // en: The Class of Device is what a Host uses to pick an icon and, on some
  //     Hosts, to decide whether to offer connecting at all. Peripheral major
  //     class with the gamepad minor value.
  // ja: Class of DeviceはHostがiconを選び、機種によっては接続を提案するかどうかを
  //     決める値である。Peripheral major classにgamepadのminor値を組み合わせる。
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x02;
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
  bluetooth.hidDevice().onDisconnected(
    [](const EspBleClassicHidConnection &) {
      Serial.println("Host disconnected");
    });

  Serial.println("Pair from the Host, then send 'a' 'b' 'd' 'r'.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'a')
    {
      // en: Buttons are a bit field, so several can be held at once. Bit 0 is
      //     button 1 as the Report Descriptor numbers them.
      // ja: buttonはbit fieldなので同時押しを表せる。bit 0がReport Descriptorの
      //     button 1にあたる。
      bluetooth.hidGamepad().send(
        0, 0, 0, 0, 0, 0, ESP_BLE_HID_GAMEPAD_HAT_CENTER, 0x0001);
    }
    else if (command == 'b')
    {
      bluetooth.hidGamepad().send(
        0, 0, 0, 0, 0, 0, ESP_BLE_HID_GAMEPAD_HAT_CENTER, 0x0002);
    }
    else if (command == 'd')
    {
      // en: The left stick pushed up-right while the hat points up. Axes are
      //     signed and centred at zero, so a stick at rest sends 0.
      // ja: 左stickを右上へ倒し、hatは上を指す。軸は符号付きで中央が0なので、
      //     倒していないstickは0を送る。
      EspBleHidGamepadReport report;
      report.x = 96;
      report.y = -96;
      report.hat = ESP_BLE_HID_GAMEPAD_HAT_UP;
      report.buttons = 0x0001;
      bluetooth.hidGamepad().sendReport(report);
    }
    else if (command == 'r')
    {
      // en: Nothing pressed and every axis centred. A Host keeps showing the
      //     last report it received, so this is what stops the input.
      // ja: 何も押さず全軸中央。HostはInput Reportを最後の値として保持し続けるため、
      //     入力を止めるにはこれを送る。
      bluetooth.hidGamepad().releaseAll();
    }
  }

  // en: Connection events are delivered from this update().
  // ja: 接続イベントはこの update() から配送される。
  bluetooth.update();
  delay(1);
}
