// en: Connect by address and receive raw Input Reports
// ja: アドレス指定で接続しraw Input Reportを受信
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  bluetooth.hidHost().onConnected([](const EspBleClassicHidConnection &event) {
    Serial.printf("HID device connected: %s\n", event.peerAddress.c_str());
  });
  bluetooth.hidHost().onConnectionFailed(
    [](const EspBleClassicHidConnectionFailure &failure) {
      Serial.printf(
        "HID connection failed: %s (%s)\n",
        failure.peerAddress.c_str(), failure.detail.c_str());
    });
  bluetooth.hidHost().onInputReport([](const EspBleClassicHidReport &event) {
    Serial.printf("Input report ID %u:", event.reportId);
    for (size_t index = 0; index < event.value.length(); ++index)
      Serial.printf(" %02x", static_cast<uint8_t>(event.value[index]));
    Serial.println();
  });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic HID Host";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!bluetooth.begin(config) || !bluetooth.hidHost().begin())
    Serial.println(bluetooth.lastErrorDetail());
  else
    Serial.println("Enter the HID device address, for example aa:bb:cc:dd:ee:ff");
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  if (Serial.available())
  {
    String address = Serial.readStringUntil('\n');
    address.trim();
    if (!bluetooth.hidHost().connect(address.c_str()))
      Serial.println(bluetooth.lastErrorDetail());
  }
  delay(1);
}
