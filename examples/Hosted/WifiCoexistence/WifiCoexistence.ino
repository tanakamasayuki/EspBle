// en: Wi-Fi and BLE at the same time on ESP32-P4, which has no radio of its own
//     and reaches both through an ESP32-C6 over ESP-Hosted. The two share one
//     transport, so the order of starting and stopping them matters in a way it
//     does not on a SoC with a built-in controller.
// ja: ESP32-P4でWi-FiとBLEを同時に使う例。P4は自前の無線を持たず、ESP-Hosted経由で
//     ESP32-C6を通して両方を使う。1つのtransportを共有するため、内蔵controllerを
//     持つSoCとは違い、開始と停止の順序が意味を持つ。
#include <EspBle.h>

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)

#include <WiFi.h>

// en: Fill in for your network, or pass them as compiler defines.
// ja: 自分のネットワークに合わせて書き換える。compiler defineで渡してもよい。
#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your-password"
#endif

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  // en: Wi-Fi is started first here, which also brings the shared transport up.
  //     Either order works; what matters is that the transport belongs to
  //     neither of them exclusively.
  // ja: ここではWi-Fiを先に開始し、それが共有transportも立ち上げる。順序は
  //     どちらでもよいが、transportがどちらか一方の専有物ではない点が重要である。
  if (!WiFi.STA.begin(false) || !WiFi.STA.connect(WIFI_SSID, WIFI_PASSWORD))
  {
    Serial.println("Wi-Fi failed to start");
    return;
  }
  const uint32_t deadline = millis() + 20000;
  while ((!WiFi.STA.connected() || !WiFi.STA.hasIP()) &&
         static_cast<int32_t>(millis() - deadline) < 0)
  {
    delay(100);
  }
  Serial.printf("Wi-Fi connected=%u ip=%s\n",
    WiFi.STA.connected() ? 1 : 0, WiFi.STA.localIP().toString().c_str());

  EspBleConfig config;
  config.deviceName = "EspBle Hosted Coexistence";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &result) {
    // en: Scanning while Wi-Fi carries traffic is the point of the example: both
    //     go through the same C6, so this is where contention would show.
    // ja: Wi-Fiが通信している最中にscanすることがこのexampleの主題である。両方が
    //     同じC6を通るため、競合があればここに現れる。
    Serial.printf("Found %s rssi=%d (wifi still connected=%u)\n",
      result.address.c_str(), result.rssi,
      WiFi.STA.connected() ? 1 : 0);
  });
  ble.scanner().start();

  Serial.println("Send 'b' to stop BLE only, 'w' to stop Wi-Fi as well.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'b')
    {
      // en: end() releases what BLE owns and leaves the transport and Wi-Fi
      //     running, because Wi-Fi is still using them.
      // ja: end()はBLEが持っている分だけを解放し、transportとWi-Fiは動かしたまま
      //     にする。Wi-Fiがまだ使っているためである。
      ble.end();
      Serial.printf("BLE stopped, wifi_connected=%u\n",
        WiFi.STA.connected() ? 1 : 0);
    }
    else if (command == 'w')
    {
      // en: The transport is released when the last user of it goes away, which
      //     is why stopping Wi-Fi last is what finally frees it.
      // ja: transportは最後の利用者が居なくなった時点で解放される。だからWi-Fiを
      //     最後に止めることで初めて解放される。
      ble.end();
      WiFi.STA.disconnect(false, 5000);
      Serial.printf("Wi-Fi stopped=%u\n", WiFi.STA.end() ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}

#else // CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE

// en: Only an ESP-Hosted target shares a transport between Wi-Fi and BLE. On a
//     SoC with its own controller there is nothing here to demonstrate, so the
//     sketch says so rather than failing to build.
// ja: transportをWi-FiとBLEで共有するのはESP-Hosted構成だけである。自前の
//     controllerを持つSoCでは示すものが無いため、buildを失敗させずにそう伝える。
void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println(
    "This example needs an ESP-Hosted target such as ESP32-P4 with an ESP32-C6.");
}

void loop()
{
  delay(1000);
}

#endif
