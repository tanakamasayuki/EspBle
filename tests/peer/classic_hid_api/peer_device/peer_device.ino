// Classic HID Host that decodes what the device sends, using the same event
// shapes the BLE host delivers.
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.hidHost().onConnected(
    [](const EspBleClassicHidConnection &connection) {
      Serial.printf("HOST_CONNECTED peer=%s\n", connection.peerAddress.c_str());
    });
  bluetooth.hidHost().onKeyboard(
    [](const EspBleClassicHidKeyboardEvent &event) {
      Serial.printf(
        "HOST_KEY usage=%u ascii=%u pressed=%u released=%u modifiers=%u raw=%u\n",
        event.usage, event.ascii, event.pressed ? 1 : 0,
        event.released ? 1 : 0, event.modifiers,
        static_cast<unsigned>(event.rawLength));
    });
  bluetooth.hidHost().onKeyboardState(
    [](const EspBleClassicHidKeyboardState &state) {
      Serial.printf("HOST_STATE modifiers=%u a=%u\n",
        state.modifiers, state.isDown(0x04) ? 1 : 0);
    });
  bluetooth.hidHost().onMouse([](const EspBleClassicHidMouseEvent &event) {
    Serial.printf("HOST_MOUSE x=%d y=%d wheel=%d buttons=%u moved=%u changed=%u\n",
      event.x, event.y, event.wheel, event.buttons, event.moved ? 1 : 0,
      event.buttonsChanged ? 1 : 0);
  });
  bluetooth.hidHost().onInputReport(
    [](const EspBleClassicHidReport &report) {
      Serial.printf("HOST_RAW id=%u len=%u\n", report.reportId,
        static_cast<unsigned>(report.value.length()));
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic HID Host";
  if (!bluetooth.begin(config) || !bluetooth.hidHost().begin())
  {
    Serial.printf("HOST_BEGIN_FAILED error=%s\n", bluetooth.lastErrorName());
    return;
  }
  Serial.println("HOST_READY");
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const String line = Serial.readStringUntil('\n');
    if (line.length() == 0) return;
    const char command = line[0];
    if (command == 'c')
      Serial.printf("HOST_CONNECT requested=%u\n",
        bluetooth.hidHost().connect(line.substring(1).c_str()) ? 1 : 0);
    else if (command == 'e')
      Serial.printf("HOST_LEDS sent=%u\n",
        bluetooth.hidHost().setKeyboardLeds(false, true, false) ? 1 : 0);
    else if (command == 'E')
      Serial.printf("HOST_LEDS sent=%u\n",
        bluetooth.hidHost().setKeyboardLeds(false, false, false) ? 1 : 0);
    else if (command == 'j')
    {
      bluetooth.hidHost().setKeyboardLayout(EspBleKeyboardLayout::JaJp);
      Serial.println("HOST_LAYOUT ja-JP");
    }
    else if (command == '?')
      Serial.printf("HOST_STATE connected=%u map=%u invalid=%u\n",
        bluetooth.hidHost().connected() ? 1 : 0,
        bluetooth.hidHost().reportMapKnown() ? 1 : 0,
        static_cast<unsigned>(bluetooth.hidHost().invalidInputReportCount()));
  }
  delay(1);
}
