// Peer for the core-host HID interoperability test. It links no EspBle code:
// the HOGP device is built from BLEHIDDevice, which Arduino-ESP32 ships on top
// of Bluedroid. The DUT's HID Host therefore parses a Report Map produced by a
// different stack than its own.
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_gap_ble_api.h>

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
BLECharacteristic *inputReport = nullptr;
BLECharacteristic *outputReport = nullptr;
BLEServer *server = nullptr;
bool linkUp = false;
uint16_t connectionId = 0;
unsigned ledWrites = 0;
uint8_t lastLeds = 0;

void reportReady()
{
  Serial.printf("HIDPEER_READY address=%s\n",
    BLEDevice::getAddress().toString().c_str());
  Serial.printf("HIDPEER_STATE connected=%u led_writes=%u leds=%02x\n",
    linkUp ? 1 : 0, ledWrites, lastLeds);
}

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *serverInstance, esp_ble_gatts_cb_param_t *param) override
  {
    linkUp = true;
    connectionId = param->connect.conn_id;
    Serial.printf("HIDPEER_CONNECTED id=%u\n", connectionId);
  }

  void onDisconnect(BLEServer *serverInstance) override
  {
    linkUp = false;
    Serial.println("HIDPEER_DISCONNECTED");
    BLEDevice::startAdvertising();
  }
};

class SecurityCallbacks : public BLESecurityCallbacks
{
  uint32_t onPassKeyRequest() override
  {
    return 0;
  }
  void onPassKeyNotify(uint32_t passKey) override {}
  bool onSecurityRequest() override
  {
    return true;
  }
  bool onConfirmPIN(uint32_t pin) override
  {
    return true;
  }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override
  {
    Serial.printf("HIDPEER_AUTH success=%u\n", result.success ? 1 : 0);
  }
};

class OutputCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic) override
  {
    const String value = characteristic->getValue();
    ++ledWrites;
    lastLeds = value.length() != 0 ? static_cast<uint8_t>(value[0]) : 0;
    // The LED report travels host to device, so this is the return direction of
    // the same cross-stack path the input reports take.
    Serial.printf("HIDPEER_LED count=%u value=%02x\n", ledWrites, lastLeds);
  }
};

void sendKey(uint8_t modifiers, uint8_t usage)
{
  uint8_t report[8] = {modifiers, 0, usage, 0, 0, 0, 0, 0};
  inputReport->setValue(report, sizeof(report));
  inputReport->notify();
  Serial.printf("HIDPEER_SENT modifiers=%02x usage=%02x\n", modifiers, usage);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  BLEDevice::init("EspBle CoreHost Keyboard");
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());
  BLESecurity::setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  BLESecurity::setCapability(ESP_IO_CAP_NONE);

  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  hid = new BLEHIDDevice(server);
  inputReport = hid->inputReport(0);
  outputReport = hid->outputReport(0);
  outputReport->setCallbacks(new OutputCallbacks());
  hid->manufacturer()->setValue("EspBle interop");
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap(const_cast<uint8_t *>(ReportMap), sizeof(ReportMap));
  hid->setBatteryLevel(77);
  hid->startServices();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(hid->hidService()->getUUID());
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  reportReady();
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
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
      server->disconnect(connectionId);
      Serial.println("HIDPEER_DISCONNECT requested=1");
    }
  }
  delay(10);
}
