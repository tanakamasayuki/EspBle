// en: Report ID 6 Vendor Input / Output / Feature
// ja: Report ID 6のVendor Input / Output / Feature
#include <EspBle.h>

EspBle ble;
uint8_t counter = 0;

static void printReport(const char *label, const EspBleHidVendorReport &report)
{
  Serial.printf("%s type=%u length=%u data=", label, report.reportType,
    static_cast<unsigned>(report.length));
  for (size_t index = 0; index < report.length; ++index)
    Serial.printf("%s%02x", index == 0 ? "" : " ", report.data[index]);
  Serial.println();
}

void setup()
{
  Serial.begin(115200);

  EspBleHidVendorConfig vendorConfig;
  vendorConfig.reportSize = 8;
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
  if (!ble.hidVendor().configure(vendorConfig))
  {
    Serial.printf("Vendor HID configuration failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }
  ble.hidVendor().onOutputReport([](const EspBleHidVendorReport &report) {
    printReport("Output", report);
  });
  ble.hidVendor().onFeatureReport([](const EspBleHidVendorReport &report) {
    printReport("Feature", report);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Vendor HID";
  config.preferredMtu = 100;
  config.security.enabled = true;
  config.security.bonding = true;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName(config.deviceName);
  ble.advertising().start();
  Serial.println("Send 'i' to send an 8-byte Vendor Input Report.");
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 'i')
  {
    const uint8_t report[] = {'E', 'S', 'P', counter++, 4, 5, 6, 7};
    Serial.printf("Input: %s\n",
      // en: Keep the report ID and payload layout consistent with the configured descriptor.
      // ja: Report IDとデータの並びを、設定したDescriptorに一致させます。
      ble.hidVendor().sendInput(report, sizeof(report)) ? "sent" : "failed");
  }
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
