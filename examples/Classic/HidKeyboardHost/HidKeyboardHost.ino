// en: A Classic HID host that receives decoded key and mouse events. The
//     callbacks and event types are the ones the BLE host uses
//     (examples/Hid/KeyboardHost); only the radio and the addressing differ.
// ja: 復号済みのkeyboard / mouse eventを受け取るClassic HID Host。callbackと
//     event型はBLE host（examples/Hid/KeyboardHost）と同じで、違うのは無線と
//     相手の指定方法だけ。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

// en: Classic has no advertisement to filter, so the peer is named by address.
//     Inquiry (examples/Classic/Inquiry) is where an address comes from.
// ja: Classicには絞り込むadvertisementが無いため、相手はaddressで指定する。
//     addressの入手はinquiry（examples/Classic/Inquiry）で行う。
const char *keyboardAddress = "00:00:00:00:00:00";

void setup()
{
  Serial.begin(115200);

  auto &keyboard = bluetooth.hidHost();

  // en: Layout-independent 256-bit usage snapshot, delivered before the
  //     per-usage events of the same report.
  // ja: layout非依存の256-bit usage snapshot。同じreportのusage単位eventより
  //     先に配送される。
  keyboard.onKeyboardState([](const EspBleClassicHidKeyboardState &state) {
    Serial.printf("Keyboard state: modifiers=0x%02x A=%u\n",
      state.modifiers, state.isDown(0x04) ? 1 : 0);
  });

  // en: The layout belongs to this side: it decides which character a usage
  //     stands for. The device chose the usage with its own layout.
  // ja: layoutはこちら側のもので、usageをどの文字と解釈するかを決める。
  //     usageの選択はdeviceが自分のlayoutで行っている。
  keyboard.setKeyboardLayout(EspBleKeyboardLayout::EnUs);

  keyboard.onKeyboard([](const EspBleClassicHidKeyboardEvent &event) {
    if (!event.pressed) return;
    Serial.printf("Key pressed: usage=0x%02x ascii=0x%02x\n",
      event.usage, event.ascii);
  });

  // en: Decoded from the descriptor's field positions, so a device that orders
  //     or sizes its fields differently still arrives here.
  // ja: descriptorのfield位置から復号するため、fieldの順序やbit数が異なる
  //     deviceでもここへ届く。
  keyboard.onMouse([](const EspBleClassicHidMouseEvent &event) {
    Serial.printf("Mouse: x=%d y=%d wheel=%d buttons=0x%02x\n",
      event.x, event.y, event.wheel, event.buttons);
  });

  // en: Reports this library cannot classify still reach the sketch raw, with
  //     the report ID in front of the payload.
  // ja: このlibraryが分類できないreportも、payloadの前にreport IDを付けた
  //     生の形でsketchへ届く。
  keyboard.onInputReport([](const EspBleClassicHidReport &report) {
    Serial.printf("Raw report: id=%u length=%u\n",
      report.reportId, static_cast<unsigned>(report.value.length()));
  });

  keyboard.onConnected([](const EspBleClassicHidConnection &connection) {
    Serial.printf("Connected to %s\n", connection.peerAddress.c_str());
  });
  keyboard.onConnectionFailed(
    [](const EspBleClassicHidConnectionFailure &failure) {
      Serial.printf("Connection to %s failed: %s\n",
        failure.peerAddress.c_str(), failure.detail.c_str());
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Keyboard Host";
  if (!bluetooth.begin(config) || !bluetooth.hidHost().begin())
  {
    Serial.printf("Classic initialization failed: %s\n",
      bluetooth.lastErrorDetail().c_str());
    return;
  }
  bluetooth.hidHost().connect(keyboardAddress);
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    // en: The LED report needs the peer's Report Descriptor, which arrives
    //     with the connection, so it is refused before then.
    // ja: LED reportは相手のReport Descriptorを必要とする。descriptorは接続と
    //     ともに届くため、それ以前は拒否される。
    if (command == 'c')
      bluetooth.hidHost().setKeyboardLeds(false, true, false);
    else if (command == '0')
      bluetooth.hidHost().setKeyboardLeds(false, false, false);
  }

  // en: Every event above is delivered from this update().
  // ja: 上記のeventはすべてこの update() から配送される。
  bluetooth.update();
  delay(1);
}
