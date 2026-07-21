// en: CustomDevice - build a HID device with an ARBITRARY Report Descriptor using
//     ble.hidCustom(). This example is a vendor-defined "control panel": a 2-byte
//     input report (a signed dial delta + a button bitfield) and a 1-byte output
//     report (an LED state written by the host). Custom reports are composed into
//     the same HID service, so they can coexist with ble.hidKeyboard()/hidMouse().
// ja: CustomDevice - ble.hidCustom() で任意のReport DescriptorのHIDデバイスを作る。
//     この例はvendor定義の「コントロールパネル」: 2byteの入力Report（符号付きダイヤル差分
//     ＋ボタンbit）と1byteの出力Report（HostがLED状態を書き込む）。カスタムReportは
//     同じHID Serviceに合成されるので、ble.hidKeyboard()/hidMouse() とも共存できる。
#include <EspBle.h>

// Vendor-defined usage page 0xFF00, Report ID 1: a 2-byte input and a 1-byte output.
static const uint8_t reportDescriptor[] = {
  0x06, 0x00, 0xFF,   // Usage Page (Vendor-Defined 0xFF00)
  0x09, 0x01,         // Usage (0x01)
  0xA1, 0x01,         // Collection (Application)
  0x85, 0x01,         //   Report ID (1)
  0x15, 0x81,         //   Logical Minimum (-127)
  0x25, 0x7F,         //   Logical Maximum (127)
  0x75, 0x08,         //   Report Size (8)
  0x95, 0x02,         //   Report Count (2)  -> 2-byte input
  0x09, 0x02,         //   Usage (0x02)
  0x81, 0x02,         //   Input (Data, Variable, Absolute)
  0x15, 0x00,         //   Logical Minimum (0)
  0x25, 0x01,         //   Logical Maximum (1)
  0x75, 0x08,         //   Report Size (8)
  0x95, 0x01,         //   Report Count (1)  -> 1-byte output
  0x09, 0x03,         //   Usage (0x03)
  0x91, 0x02,         //   Output (Data, Variable, Absolute)
  0xC0,               // End Collection
};

EspBle ble;

void setup()
{
  Serial.begin(115200);

  // en: Configure the custom HID device before begin(): set the raw Report Map
  //     and declare each report (id, byte size). Report IDs 1..6 are reserved for
  //     the built-in profiles only while one of them is also enabled.
  // ja: begin() 前にカスタムHIDを構成する: raw Report Mapを設定し、各Report（id・byte数）を
  //     宣言する。Report ID 1〜6は、対応する内蔵profileを有効にしている場合のみ予約される。
  auto &custom = ble.hidCustom();
  custom.configure();
  custom.addInputReport(1, 2);
  custom.addOutputReport(1, 1);
  custom.setReportMap(reportDescriptor, sizeof(reportDescriptor));

  // en: The host writes the 1-byte output report (e.g. an LED state) here.
  // ja: Hostが1byteの出力Report（例: LED状態）をここで書き込む。
  custom.onOutputReport([](const EspBleHidVendorReport &report) {
    Serial.printf("Output report id=%u len=%u value=%u\n",
      report.reportId, static_cast<unsigned>(report.length),
      report.length > 0 ? report.data[0] : 0);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Custom HID";
  // en: HOGP normally requires an encrypted link; enable security for real hosts.
  // ja: HOGPは通常暗号化linkを要求する。実機Host向けにはsecurityを有効化する。
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName("EspBle Custom HID");
  ble.advertising().start();

  Serial.println("Send 'i' to send an input report (dial +5, button 1).");
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 'i')
  {
    // en: 2-byte input report: dial delta = +5, buttons = 0x01.
    // ja: 2byteの入力Report: ダイヤル差分 = +5、ボタン = 0x01。
    const uint8_t report[2] = {static_cast<uint8_t>(5), 0x01};
    ble.hidCustom().sendInput(1, report, sizeof(report));
  }
  // en: Output/feature report events are delivered from this update().
  // ja: Output/Feature Reportイベントはこの update() から配送される。
  ble.update();
  delay(1);
}
