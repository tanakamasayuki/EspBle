// en: Wi-Fi and BLE over one shared ESP-Hosted transport, and the order they stop in
// ja: 1つの共有ESP-Hosted transportでWi-FiとBLEを同時利用し、停止順序を示す
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
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
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

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
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
