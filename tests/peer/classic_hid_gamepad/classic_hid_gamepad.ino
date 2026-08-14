// The Classic HID gamepad, which is the case Classic exists for: an older
// console accepts BR/EDR HID only. Keyboard is configured alongside it to prove
// a gamepad still composes with another profile — the pair comes to 133
// descriptor bytes, where adding the mouse as well would exceed the SDP record.
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Gamepad";
  bluetooth.hidKeyboard().configure(hidConfig);
  bluetooth.hidGamepad().configure(hidConfig);

  bluetooth.hidDevice().onConnected(
    [](const EspBleClassicHidConnection &connection) {
      Serial.printf("GAMEPAD_DEVICE_CONNECTED peer=%s\n",
        connection.peerAddress.c_str());
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Gamepad";
  // Peripheral / gamepad, which is what a Host uses to pick an icon.
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x02;
  if (!bluetooth.begin(config))
  {
    Serial.printf("GAMEPAD_DEVICE_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf(
    "GAMEPAD_DEVICE_READY address=%02x:%02x:%02x:%02x:%02x:%02x gamepad=%u\n",
    address[0], address[1], address[2], address[3], address[4], address[5],
    bluetooth.hidGamepad().configured() ? 1 : 0);
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'g')
      // Signed axes and an enumerated hat, so wrong packing shows up as a
      // different value rather than as a missing report.
      Serial.printf("GAMEPAD_DEVICE_SENT sent=%u\n",
        bluetooth.hidGamepad().send(
          96, -96, 0, 0, 0, 0, ESP_BLE_HID_GAMEPAD_HAT_UP, 0x0003) ? 1 : 0);
    else if (command == 'r')
      Serial.printf("GAMEPAD_DEVICE_RELEASED sent=%u\n",
        bluetooth.hidGamepad().releaseAll() ? 1 : 0);
    else if (command == 'k')
      // The keyboard shares the device, so its report has to keep its own ID.
      Serial.printf("GAMEPAD_DEVICE_KEY sent=%u\n",
        bluetooth.hidKeyboard().pressUsage(0x04) ? 1 : 0);
    else if (command == '?')
      Serial.printf("GAMEPAD_DEVICE_STATE connected=%u\n",
        bluetooth.hidDevice().connected() ? 1 : 0);
  }
  delay(1);
}
