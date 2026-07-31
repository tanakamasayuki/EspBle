#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  auto &keyboard = ble.hidKeyboard();
  keyboard.enableNkro();
  keyboard.configure();

  EspBleConfig config;
  config.deviceName = "EspBle NKRO Keyboard";
  config.preferredMtu = 64; // A 29-byte NKRO Input Report needs MTU >= 32.
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName("EspBle NKRO Keyboard");
  ble.advertising().start();
  Serial.println("Send 'n' for eight simultaneous keys, 'r' to release all.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    auto &keyboard = ble.hidKeyboard();
    // ready() is false until a host is connected, encrypted, and subscribed to
    // the Input Report. Sending before that fails with InvalidState, and a host
    // that has not arrived yet is a normal state rather than an error, so it
    // does not touch lastError().
    if (!keyboard.ready())
    {
      Serial.println("No subscribed HID Host yet.");
    }
    else if (command == 'n')
    {
      // sendReport(EspBleHidKeyboardReport) carries keys[6] and still expresses
      // only six usages with NKRO enabled, so eight keys go out as one
      // whole-state report. pressUsage() could hold eight keys too, but each
      // key change would be its own notification and the chord would be paced
      // by the connection interval.
      EspBleHidKeyboardNkroReport report;
      const uint8_t usages[] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
      for (uint8_t usage : usages) report.press(usage);
      keyboard.sendReport(report);
    }
    else if (command == 'r')
    {
      keyboard.releaseAll();
    }
  }
  ble.update();
  delay(1);
}
