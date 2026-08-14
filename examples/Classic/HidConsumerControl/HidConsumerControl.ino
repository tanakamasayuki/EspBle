// en: Bluetooth Classic (BR/EDR) media keys. Car audio units and older TVs
//     accept Classic HID, so this is the transport for them; the calls are the
//     same ones the BLE example uses (examples/Hid/ConsumerControl).
// ja: Bluetooth Classic（BR/EDR）のメディアキー。car audioや古いTVはClassic HIDを
//     受け付けるため、その相手にはこちらを使う。呼び出しはBLE example
//     （examples/Hid/ConsumerControl）と同じ。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Media";
  bluetooth.hidConsumerControl().configure(hidConfig);
  // en: System Control sends power and sleep requests. It is a separate profile
  //     because a Host treats those differently from media keys.
  // ja: System Controlは電源・スリープ要求を送る。Hostがメディアキーとは別扱いに
  //     するため、profileも別になっている。
  bluetooth.hidSystemControl().configure(hidConfig);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Media";
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

  Serial.println("Pair from the Host, then send '+' '-' 'p' 'n' 's'.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '+')
    {
      // en: click() presses and releases in one call, which is what a media key
      //     is: a Host acts on the press and needs the release to stop
      //     repeating.
      // ja: click()は1回の呼び出しで押下と解放を送る。メディアキーはこの形で、
      //     Hostは押下で動作し、解放が無いと繰り返しが止まらない。
      bluetooth.hidConsumerControl().click(
        ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP);
    }
    else if (command == '-')
    {
      bluetooth.hidConsumerControl().click(
        ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_DOWN);
    }
    else if (command == 'p')
    {
      bluetooth.hidConsumerControl().click(
        ESP_BLE_HID_CONSUMER_CONTROL_PLAY_PAUSE);
    }
    else if (command == 'n')
    {
      bluetooth.hidConsumerControl().click(
        ESP_BLE_HID_CONSUMER_CONTROL_NEXT_TRACK);
    }
    else if (command == 's')
    {
      // en: Usage 0x82 on the Generic Desktop page is Sleep. Whether a Host
      //     acts on it is the Host's decision.
      // ja: Generic Desktop pageのusage 0x82がSleep。実際に反応するかどうかは
      //     Host側の判断になる。
      bluetooth.hidSystemControl().click(0x82);
    }
  }

  bluetooth.update();
  delay(1);
}
