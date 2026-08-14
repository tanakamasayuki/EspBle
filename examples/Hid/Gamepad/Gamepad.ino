// en: A BLE HID gamepad over GATT (HOGP): six signed axes, a hat switch and 32
//     buttons. The same calls exist on the Classic side
//     (examples/Classic/HidGamepad), which is what a peer that only speaks
//     BR/EDR HID needs.
// ja: GATT（HOGP）上のBLE HID gamepad。符号付き6軸、hat switch、32 button。
//     Classic側にも同じ呼び出しがある（examples/Classic/HidGamepad）。BR/EDR HIDしか
//     受け付けない相手にはそちらを使う。
#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  // en: Profiles are composed into one HID service before begin(); a Host reads
  //     the Report Descriptor once, at discovery.
  // ja: profileはbegin()より前に1つのHID Serviceへ合成する。HostはReport Descriptorを
  //     Discovery時に一度だけ読む。
  ble.hidGamepad().configure();

  EspBleConfig config;
  config.deviceName = "EspBle Gamepad";
  // en: HOGP requires an encrypted link, so security and bonding are needed.
  // ja: HOGPは暗号化linkを要求するため、securityとbondingが必要。
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  // en: A Host that drops the link expects to find the device again, so
  //     advertising restarts on every disconnect.
  // ja: linkが切れたHostは再び見つけられることを期待するため、切断ごとに
  //     advertisingを再開する。
  ble.onDisconnected([](const EspBleConnection &) {
    ble.advertising().start();
  });
  ble.advertising().start();

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
      ble.hidGamepad().send(
        0, 0, 0, 0, 0, 0, ESP_BLE_HID_GAMEPAD_HAT_CENTER, 0x0001);
    }
    else if (command == 'b')
    {
      ble.hidGamepad().send(
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
      ble.hidGamepad().sendReport(report);
    }
    else if (command == 'r')
    {
      // en: Nothing pressed and every axis centred. A Host keeps showing the
      //     last report it received, so this is what stops the input.
      // ja: 何も押さず全軸中央。HostはInput Reportを最後の値として保持し続けるため、
      //     入力を止めるにはこれを送る。
      ble.hidGamepad().releaseAll();
    }
  }

  // en: Connection and subscription events are delivered from this update().
  // ja: 接続・購読イベントはこの update() から配送される。
  ble.update();
  delay(1);
}
