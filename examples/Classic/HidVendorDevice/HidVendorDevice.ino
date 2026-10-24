// en: Classic HID Device with an arbitrary Report Descriptor
// ja: 任意Report DescriptorのClassic HID Device
#include <EspBleClassic.h>

static const uint8_t ReportDescriptor[] = {
  0x06, 0x00, 0xff, // Usage Page (Vendor 0xff00)
  0x09, 0x01,       // Usage 1
  0xa1, 0x01,       // Collection (Application)
  0x85, 0x01,       // Report ID 1
  0x15, 0x00,       // Logical Minimum 0
  0x26, 0xff, 0x00, // Logical Maximum 255
  0x75, 0x08,       // Report Size 8
  0x95, 0x04,       // Report Count 4
  0x09, 0x01,       // Usage 1
  0x81, 0x02,       // Input (Data, Variable, Absolute)
  0xc0,             // End Collection
};

EspBleClassic bluetooth;
uint8_t counter = 0;
uint32_t nextReport = 0;

void setup()
{
  Serial.begin(115200);

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  bluetooth.hidDevice().onConnected([](const EspBleClassicHidConnection &event) {
    Serial.printf("HID host connected: %s\n", event.peerAddress.c_str());
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  bluetooth.hidDevice().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("HID host disconnected");
  });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle Classic Vendor HID";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!bluetooth.begin(stackConfig))
  {
    Serial.println(bluetooth.lastErrorDetail());
    return;
  }

  EspBleClassicHidDeviceConfig hidConfig;
  hidConfig.name = "EspBle Vendor HID";
  hidConfig.reportDescriptor = ReportDescriptor;
  hidConfig.reportDescriptorLength = sizeof(ReportDescriptor);
  if (!bluetooth.hidDevice().begin(hidConfig))
    Serial.println(bluetooth.lastErrorDetail());
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  if (
    bluetooth.hidDevice().connected() &&
    static_cast<int32_t>(millis() - nextReport) >= 0)
  {
    const uint8_t report[] = {counter++, 0x7f, 0x80, 0xff};
    // en: Keep the report ID and payload layout consistent with the configured descriptor.
    // ja: Report IDとデータの並びを、設定したDescriptorに一致させます。
    bluetooth.hidDevice().sendInputReport(1, report, sizeof(report));
    nextReport = millis() + 1000;
  }
  delay(1);
}
