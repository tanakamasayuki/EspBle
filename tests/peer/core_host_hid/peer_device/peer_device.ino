// Peer for the core-host HID interoperability test. It links no EspBle code:
// the HOGP device is built from BLEHIDDevice, which Arduino-ESP32 ships and
// which runs on Bluedroid on the original ESP32. The DUT's HID Host therefore parses a Report Map produced by a
// different stack than its own.
#include <BLE.h>

#include "../../../sketch_support/EspBleTestLifecycle.h"

// A plain 8-byte boot-style keyboard report with no report ID, which is what a
// simple HOGP device publishes: modifiers, one reserved byte, six key usages.
static const uint8_t ReportMap[] = {
  0x05, 0x01,  // Usage Page (Generic Desktop)
  0x09, 0x06,  // Usage (Keyboard)
  0xa1, 0x01,  // Collection (Application)
  0x05, 0x07,  //   Usage Page (Keyboard/Keypad)
  0x19, 0xe0,  //   Usage Minimum (Left Control)
  0x29, 0xe7,  //   Usage Maximum (Right GUI)
  0x15, 0x00,  //   Logical Minimum (0)
  0x25, 0x01,  //   Logical Maximum (1)
  0x75, 0x01,  //   Report Size (1)
  0x95, 0x08,  //   Report Count (8)
  0x81, 0x02,  //   Input (Data, Variable, Absolute)
  0x95, 0x01,  //   Report Count (1)
  0x75, 0x08,  //   Report Size (8)
  0x81, 0x01,  //   Input (Constant)
  0x95, 0x05,  //   Report Count (5)
  0x75, 0x01,  //   Report Size (1)
  0x05, 0x08,  //   Usage Page (LEDs)
  0x19, 0x01,  //   Usage Minimum (Num Lock)
  0x29, 0x05,  //   Usage Maximum (Kana)
  0x91, 0x02,  //   Output (Data, Variable, Absolute)
  0x95, 0x01,  //   Report Count (1)
  0x75, 0x03,  //   Report Size (3)
  0x91, 0x01,  //   Output (Constant)
  0x95, 0x06,  //   Report Count (6)
  0x75, 0x08,  //   Report Size (8)
  0x15, 0x00,  //   Logical Minimum (0)
  0x25, 0x65,  //   Logical Maximum (101)
  0x05, 0x07,  //   Usage Page (Keyboard/Keypad)
  0x19, 0x00,  //   Usage Minimum (0)
  0x29, 0x65,  //   Usage Maximum (101)
  0x81, 0x00,  //   Input (Data, Array)
  0xc0,        // End Collection
};

BLEHIDDevice *hid = nullptr;
BLECharacteristic inputReport;
BLECharacteristic outputReport;
BLEServer server;
bool linkUp = false;
uint16_t connectionId = 0;
unsigned ledWrites = 0;
uint8_t lastLeds = 0;
bool stackStopped = false;

void reportReady()
{
  Serial.printf("HIDPEER_READY address=%s\n",
    BLE.getAddress().toString().c_str());
  Serial.printf("HIDPEER_STATE connected=%u led_writes=%u leds=%02x\n",
    linkUp ? 1 : 0, ledWrites, lastLeds);
}

// The handlers below run on the BLE stack task.
static void onConnect(BLEServer &, const BLEConnInfo &connection)
{
  linkUp = true;
  connectionId = connection.getHandle();
  Serial.printf("HIDPEER_CONNECTED id=%u\n", connectionId);
}

static void onDisconnect(BLEServer &, const BLEConnInfo &, uint8_t)
{
  linkUp = false;
  Serial.println("HIDPEER_DISCONNECTED");
  // The server advertises again by itself (advertiseOnDisconnect).
}

static void onAuthenticationComplete(const BLEConnInfo &, bool success)
{
  Serial.printf("HIDPEER_AUTH success=%u\n", success ? 1 : 0);
}

static void onOutputWrite(BLECharacteristic &characteristic, const BLEConnInfo &)
{
  size_t length = 0;
  const uint8_t *value = characteristic.getValue(&length);
  ++ledWrites;
  lastLeds = value != nullptr && length != 0 ? value[0] : 0;
  // The LED report travels host to device, so this is the return direction of
  // the same cross-stack path the input reports take.
  Serial.printf("HIDPEER_LED count=%u value=%02x\n", ledWrites, lastLeds);
}

void sendKey(uint8_t modifiers, uint8_t usage)
{
  uint8_t report[8] = {modifiers, 0, usage, 0, 0, 0, 0, 0};
  inputReport.setValue(report, sizeof(report));
  inputReport.notify();
  Serial.printf("HIDPEER_SENT modifiers=%02x usage=%02x\n", modifiers, usage);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  const BTStatus began = BLE.begin("EspBle CoreHost Keyboard");
  if (!began)
  {
    Serial.printf("HIDPEER_INIT_FAILED stage=begin status=%s\n", began.toString());
    return;
  }
  BLESecurity security = BLE.getSecurity();
  // Just Works: accept every request and confirmation, as the old callbacks did.
  security.onPassKeyRequest([](const BLEConnInfo &) -> uint32_t { return 0; });
  security.onSecurityRequest([](const BLEConnInfo &) { return true; });
  security.onConfirmPassKey([](const BLEConnInfo &, uint32_t) { return true; });
  security.onAuthenticationComplete(onAuthenticationComplete);
  // Bonding and LE Secure Connections without MITM (ESP_LE_AUTH_REQ_SC_BOND).
  security.setAuthenticationMode(true, false, true);
  security.setIOCapability(BLEIOCapability::NoInputNoOutput);

  server = BLE.createServer();
  server.onConnect(onConnect);
  server.onDisconnect(onDisconnect);
  server.advertiseOnDisconnect(true);

  hid = new BLEHIDDevice(server);
  inputReport = hid->inputReport(0);
  outputReport = hid->outputReport(0);
  // The reports stay behind encryption, as the Bluedroid BLEHIDDevice of the
  // 3.x core published them; the library now creates them open.
  inputReport.setPermissions(BLEPermission::ReadEncrypted);
  outputReport.setPermissions(BLEPermission::ReadWriteEncrypted);
  outputReport.onWrite(onOutputWrite);
  hid->manufacturer("EspBle interop");
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap(ReportMap, sizeof(ReportMap));
  hid->setBatteryLevel(77);
  const BTStatus started = server.start();
  if (!started)
  {
    Serial.printf("HIDPEER_INIT_FAILED stage=server status=%s\n", started.toString());
    return;
  }

  // BLEHIDDevice has already put the HID service UUID into the advertising data.
  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  advertising.setScanResponse(true);
  const BTStatus advertising_started = BLE.startAdvertising();
  if (!advertising_started)
  {
    Serial.printf("HIDPEER_INIT_FAILED stage=advertise status=%s\n", advertising_started.toString());
    return;
  }

  reportReady();

  // onDisconnect advertises again on its own, so RECOVER only drops the link.
  // end() takes the radio down synchronously, so STOP is complete at once.
  EspBleTestLifecycle::Hooks hooks;
  hooks.stop = []() {
    stackStopped = true;
    BLE.end(false);
  };
  EspBleTestLifecycle::begin(hooks);
}

void loop()
{
  EspBleTestLifecycle::update();
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (EspBleTestLifecycle::handleLine(command)) return;
    // STOP has taken the stack down; the commands have nothing left to act on.
    if (stackStopped) return;
    if (command == "?")
    {
      reportReady();
    }
    else if (command == "a")
    {
      sendKey(0x00, 0x04);  // 'a'
    }
    else if (command == "A")
    {
      sendKey(0x02, 0x04);  // Left Shift + 'a'
    }
    else if (command == "z")
    {
      sendKey(0x00, 0x00);  // all released
    }
    else if (command == "d" && linkUp)
    {
      server.disconnect(connectionId);
      Serial.println("HIDPEER_DISCONNECT requested=1");
    }
  }
  delay(10);
}
