// en: Media keys and system requests
// ja: メディアキーとシステム要求
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Media";
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
  bluetooth.hidConsumerControl().configure(hidConfig);
  // en: System Control sends power and sleep requests. It is a separate profile
  //     because a Host treats those differently from media keys.
  // ja: System Controlは電源・スリープ要求を送る。Hostがメディアキーとは別扱いに
  //     するため、profileも別になっている。
  bluetooth.hidSystemControl().configure(hidConfig);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Media";
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

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  delay(1);
}
