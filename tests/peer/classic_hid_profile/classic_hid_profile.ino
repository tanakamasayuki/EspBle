// Classic HID Device driven through the same profile API the BLE side uses.
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic HID Profile";
  bluetooth.hidKeyboard().configure(hidConfig);
  bluetooth.hidMouse().configure(hidConfig);
  bluetooth.hidConsumerControl().configure(hidConfig);

  bluetooth.hidKeyboard().onOutputReport(
    [](const EspBleClassicHidKeyboardLeds &leds) {
      Serial.printf("DEVICE_LEDS peer=%s leds=%u caps=%u num=%u\n",
        leds.peerAddress.c_str(), leds.leds, leds.capsLock ? 1 : 0,
        leds.numLock ? 1 : 0);
    });
  bluetooth.hidDevice().onConnected(
    [](const EspBleClassicHidConnection &connection) {
      Serial.printf("DEVICE_CONNECTED peer=%s\n",
        connection.peerAddress.c_str());
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic HID Profile";
  if (!bluetooth.begin(config))
  {
    Serial.printf("DEVICE_BEGIN_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf(
    "DEVICE_READY address=%02x:%02x:%02x:%02x:%02x:%02x keyboard=%u mouse=%u\n",
    address[0], address[1], address[2], address[3], address[4], address[5],
    bluetooth.hidKeyboard().configured() ? 1 : 0,
    bluetooth.hidMouse().configured() ? 1 : 0);
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'k')
      Serial.printf("DEVICE_KEY sent=%u\n",
        bluetooth.hidKeyboard().pressUsage(0x04) ? 1 : 0);
    else if (command == 'r')
      Serial.printf("DEVICE_RELEASE sent=%u\n",
        bluetooth.hidKeyboard().releaseAll() ? 1 : 0);
    else if (command == 'm')
      Serial.printf("DEVICE_MOUSE sent=%u buttons=%u\n",
        bluetooth.hidMouse().move(5, -3) ? 1 : 0,
        bluetooth.hidMouse().buttons());
    else if (command == 'c')
      Serial.printf("DEVICE_CLICK sent=%u buttons=%u\n",
        bluetooth.hidMouse().press(ESP_BLE_HID_MOUSE_LEFT) ? 1 : 0,
        bluetooth.hidMouse().buttons());
    else if (command == 'v')
      Serial.printf("DEVICE_RELEASE_BUTTONS sent=%u buttons=%u\n",
        bluetooth.hidMouse().releaseAll() ? 1 : 0,
        bluetooth.hidMouse().buttons());
    else if (command == 'u')
      Serial.printf("DEVICE_CONSUMER sent=%u\n",
        bluetooth.hidConsumerControl().sendUsage(
          ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP) ? 1 : 0);
    else if (command == 'l')
    {
      // Layout only changes which usage a character maps to; the report the
      // Host receives is still a usage, which is what the test checks.
      bluetooth.hidKeyboard().setLayout(EspBleKeyboardLayout::JaJp);
      Serial.printf("DEVICE_LAYOUT ja=%u\n",
        bluetooth.hidKeyboard().layout() == EspBleKeyboardLayout::JaJp ? 1 : 0);
    }
    else if (command == 'q')
      Serial.printf("DEVICE_QUOTE sent=%u\n",
        bluetooth.hidKeyboard().pressKey('"') ? 1 : 0);
    else if (command == '?')
      Serial.printf("DEVICE_STATE connected=%u ready=%u heap=%u\n",
        bluetooth.hidDevice().connected() ? 1 : 0,
        bluetooth.hidKeyboard().ready() ? 1 : 0,
        static_cast<unsigned>(ESP.getFreeHeap()));
  }
  delay(1);
}
