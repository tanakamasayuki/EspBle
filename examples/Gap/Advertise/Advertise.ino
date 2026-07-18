// en: Advertise - start connectable legacy advertising carrying a device name and a
//     service UUID. Minimal peripheral example; observe it with a scanner app or the
//     Gap/Scan example.
// ja: Advertise - デバイス名とService UUIDを載せたconnectableなLegacy Advertisingを開始する。
//     Peripheral側の最小例。汎用BLEスキャナアプリやGap/Scan exampleで受信できる。
#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  // en: Configure stack settings (device name, etc.) before begin().
  // ja: begin() 前にデバイス名などのスタック設定を行う。
  EspBleConfig config;
  config.deviceName = "EspBle Advertiser"; // en: GAP device name / ja: GAPデバイス名
  if (!ble.begin(config))
  {
    // en: The failure reason is available via lastErrorName()/lastErrorDetail().
    // ja: 失敗理由は lastErrorName()/lastErrorDetail() で取得できる。
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  // en: Build the advertising payload. Service UUIDs are merged into one Complete
  //     List AD structure per UUID size (CSS Part A 1.1).
  // ja: Advertising payloadを構築する。Service UUIDはサイズごとに単一の
  //     Complete List AD構造へまとめられる（CSS Part A 1.1）。
  auto &advertising = ble.advertising();
  advertising.setName("EspBle Advertiser");     // en: put the local name in the payload / ja: local nameを載せる
  advertising.addServiceUuid("1812");           // en: HID Service UUID (16-bit) / ja: HID Service UUID（16-bit表記）
  if (!advertising.start())
  {
    // en: Fails with InvalidArgument if the payload would exceed 31 bytes.
    // ja: payloadが31byteを超える等の場合は InvalidArgument で失敗する。
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  // en: Advertising continues internally, but call update() regularly for event delivery.
  // ja: Advertising自体は内部で継続するが、イベント配送のため update() は定期的に呼ぶ。
  ble.update();
  delay(1);
}
