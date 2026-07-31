#include <EspBle.h>

EspBle ble;
bool outputCallbackInstalled = false;

static EspBleConfig makeConfig(uint16_t preferredMtu)
{
  EspBleConfig config;
  config.deviceName = "EspBle NKRO Peer";
  config.preferredMtu = preferredMtu;
  config.security.enabled = true;
  config.security.bonding = true;
  return config;
}

static void startAdvertising()
{
  ble.advertising().setName("EspBle NKRO Peer");
  ble.advertising().start();
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  auto &keyboard = ble.hidKeyboard();
  keyboard.enableNkro();
  keyboard.configure();
  keyboard.onOutputReport([](const EspBleHidKeyboardOutputReport &report) {
    Serial.printf("DEVICE_OUTPUT leds=%u\n", report.leds);
  });
  outputCallbackInstalled = true;

  if (!ble.begin(makeConfig(64))) return;
  startAdvertising();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
      Serial.printf("DEVICE_BONDS_CLEARED success=%u\n", ble.deleteAllBonds() ? 1 : 0);
    else if (command == 'n')
    {
      const uint8_t usages[] = {0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x87};
      bool success = true;
      for (uint8_t usage : usages)
      {
        success = ble.hidKeyboard().pressUsage(usage) && success;
        delay(25);
      }
      Serial.printf("DEVICE_NKRO_SENT success=%u\n", success ? 1 : 0);
    }
    else if (command == 'w')
    {
      // The whole NKRO state in one notification. The keys[6] overload of
      // sendReport() cannot express this: it carries six usages even with NKRO
      // enabled. LeftShift (0xE1) is above the 0x00-0xDF bitmap, so press()
      // routes it into `modifiers` instead.
      EspBleHidKeyboardNkroReport report;
      const uint8_t usages[] = {0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x87,0xe1};
      bool represented = true;
      for (uint8_t usage : usages) represented = report.press(usage) && represented;
      Serial.printf("DEVICE_NKRO_STATE_SENT success=%u represented=%u modifiers=%u\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0,
        represented ? 1 : 0,
        report.modifiers);
    }
    else if (command == 'e')
    {
      // ledState() must answer with what the host wrote, without the sketch
      // caching the onOutputReport() callback into a variable of its own.
      const EspBleHidKeyboardOutputReport led = ble.hidKeyboard().ledState();
      Serial.printf("DEVICE_LED_STATE leds=%u num=%u caps=%u scroll=%u connection=%u\n",
        led.leds,
        led.numLock ? 1 : 0,
        led.capsLock ? 1 : 0,
        led.scrollLock ? 1 : 0,
        static_cast<unsigned>(led.connectionId));
    }
    else if (command == 'u')
    {
      // Drop the output-report callback. ledState() must keep following the
      // host afterwards: dispatchPendingOutputReports() returns early without a
      // callback, so nothing drains the output queue and it stays full — the
      // saved state has to be updated before that queue, not at dispatch.
      ble.hidKeyboard().onOutputReport(nullptr);
      outputCallbackInstalled = false;
      Serial.printf("DEVICE_OUTPUT_CALLBACK installed=%u\n",
        outputCallbackInstalled ? 1 : 0);
    }
    else if (command == 'h')
    {
      // heldState() must be what the host was last told, whichever API set it:
      // the whole-state overload, the 6-key overload, or the incremental
      // pressUsage() / releaseUsage(). A caller that builds the state itself
      // compares against this to skip an unchanged report.
      const EspBleHidKeyboardNkroReport &held = ble.hidKeyboard().heldState();
      unsigned count = 0;
      for (unsigned usage = 0; usage <= 0xff; ++usage)
        if (held.isDown(static_cast<uint8_t>(usage))) ++count;
      Serial.printf("DEVICE_HELD count=%u a=%u high=%u shift=%u modifiers=%u\n",
        count,
        held.isDown(0x04) ? 1 : 0,
        held.isDown(0x87) ? 1 : 0,
        held.isDown(0xe1) ? 1 : 0,
        held.modifiers);
    }
    else if (command == 'b')
      Serial.printf("DEVICE_RELEASE_USAGE success=%u\n",
        ble.hidKeyboard().releaseUsage(0x05) ? 1 : 0);
    else if (command == 'r')
      Serial.printf("DEVICE_RELEASE_ALL success=%u\n",
        ble.hidKeyboard().releaseAll() ? 1 : 0);
    else if (command == 'm')
    {
      // An NKRO report is 29 bytes, so it needs MTU >= 32 (29 + the 3-byte ATT
      // header). begin() refuses a lower preferredMtu rather than letting every
      // report notify fail silently later. Walk the boundary: the spec minimum
      // (23), one below the limit (31), then the limit itself (32).
      ble.end();
      const bool minimumRejected = ble.begin(makeConfig(23));
      Serial.printf("DEVICE_MTU_23 success=%u error=%s detail=%s\n",
        minimumRejected ? 1 : 0, ble.lastErrorName(), ble.lastErrorDetail().c_str());
      const bool belowRejected = ble.begin(makeConfig(31));
      Serial.printf("DEVICE_MTU_31 success=%u error=%s\n",
        belowRejected ? 1 : 0, ble.lastErrorName());
      const bool limitAccepted = ble.begin(makeConfig(32));
      Serial.printf("DEVICE_MTU_32 success=%u error=%s\n",
        limitAccepted ? 1 : 0, ble.lastErrorName());
      // Back to the configuration the rest of this suite runs with.
      ble.end();
      const bool restored = ble.begin(makeConfig(64));
      if (restored) startAdvertising();
      Serial.printf("DEVICE_MTU_RESTORED success=%u nkro=%u\n",
        restored ? 1 : 0, ble.hidKeyboard().nkroEnabled() ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
