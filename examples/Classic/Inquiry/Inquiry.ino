// en: Classic device discovery. Every other Classic profile connects by
//     address, so this is where an address comes from when the sketch does not
//     already know one.
// ja: Classicのdevice discovery。他のClassic profileはaddress指定で接続するため、
//     addressを知らない場合の入手経路がここになる。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Inquiry";
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  // en: One result per response. A peer that answers repeatedly is reported
  //     repeatedly; deduplicate by address if that matters to the sketch.
  // ja: 応答1件につき1 result。同じpeerが複数回応答すれば複数回届くので、
  //     必要ならaddressで重複を除く。
  bluetooth.inquiry().onResult([](const EspBleClassicInquiryResult &result) {
    Serial.printf("Found %s", result.address.c_str());
    if (!result.name.isEmpty()) Serial.printf(" name=%s", result.name.c_str());
    if (result.hasClassOfDevice)
      Serial.printf(" cod=0x%06x", static_cast<unsigned>(result.classOfDevice));
    if (result.hasRssi) Serial.printf(" rssi=%d", result.rssi);
    Serial.println();
  });

  bluetooth.inquiry().onComplete([](const EspBleClassicInquiryComplete &event) {
    Serial.printf(
      "Inquiry finished (%s)\n", event.cancelled ? "cancelled" : "timed out");
  });

  EspBleClassicInquiryConfig inquiryConfig;
  // en: The controller counts in 1.28 s units, so this rounds up to 10.24 s.
  // ja: controllerは1.28 s単位で数えるため、10.24 sへ切り上げられる。
  inquiryConfig.durationSeconds = 10;
  if (!bluetooth.inquiry().start(inquiryConfig))
  {
    Serial.printf(
      "Inquiry failed to start: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
  }
}

void loop()
{
  // en: Results and the completion event are delivered from update().
  // ja: resultと完了eventはupdate()から配送される。
  bluetooth.update();
  delay(10);
}
