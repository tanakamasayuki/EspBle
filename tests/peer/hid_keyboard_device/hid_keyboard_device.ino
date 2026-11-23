#include <Arduino.h>
#include <BLE.h>

#include "../../sketch_support/EspBleTestLifecycle.h"

static const BLEUUID HID_SERVICE_UUID((uint16_t)0x1812);
static const BLEUUID REPORT_UUID((uint16_t)0x2a4d);
static const BLEUUID REPORT_MAP_UUID((uint16_t)0x2a4b);
static const BLEUUID REPORT_REFERENCE_UUID((uint16_t)0x2908);
static const BLEUUID BATTERY_SERVICE_UUID((uint16_t)0x180f);
static const BLEUUID BATTERY_LEVEL_UUID((uint16_t)0x2a19);

BLEAdvertisedDevice target;
volatile bool targetFound = false;
BLEClient client;
BLERemoteCharacteristic inputReport;
BLERemoteCharacteristic outputReport;
BLERemoteCharacteristic mouseInputReport;
volatile bool securityComplete = false;
bool stackStopped = false;

// Runs on the BLE stack task.
static void onScanResult(const BLEAdvertisedDevice &device)
{
  if (!targetFound && device.haveServiceUUID() && device.isAdvertisingService(HID_SERVICE_UUID))
  {
    target = device;
    targetFound = true;
  }
}

// Runs on the BLE stack task once the link is encrypted (or encryption failed).
static void onAuthenticationComplete(const BLEConnInfo &connection, bool)
{
  securityComplete = connection.isEncrypted();
  Serial.printf("PEER_SECURITY encrypted=%u bonded=%u\n",
    connection.isEncrypted() ? 1 : 0, connection.isBonded() ? 1 : 0);
}

static void clearBonds()
{
  BLESecurity security = BLE.getSecurity();
  security.deleteAllBonds();
  const int count = static_cast<int>(security.getBondedDevices().size());
  Serial.printf("PEER_BONDS_CLEARED count=%d\n", count);
}

static void inputNotification(
  BLERemoteCharacteristic &, const uint8_t *data, size_t length, BLESubscription)
{
  Serial.printf(
    "INPUT_REPORT length=%u modifiers=%u key0=%u\n",
    static_cast<unsigned>(length),
    length > 0 ? data[0] : 0,
    length > 2 ? data[2] : 0);
}

static void mouseNotification(
  BLERemoteCharacteristic &, const uint8_t *data, size_t length, BLESubscription)
{
  Serial.printf("MOUSE_REPORT length=%u buttons=%u x=%d y=%d wheel=%d\n",
    static_cast<unsigned>(length), length > 0 ? data[0] : 0,
    length > 1 ? static_cast<int8_t>(data[1]) : 0,
    length > 2 ? static_cast<int8_t>(data[2]) : 0,
    length > 3 ? static_cast<int8_t>(data[3]) : 0);
}

static bool connectAndDiscover()
{
  targetFound = false;
  BLEScan scan = BLE.getScan();
  scan.onResult(onScanResult);
  scan.setFilterDuplicates(false);
  scan.setActiveScan(true);
  // Wait here for onScanResult (up to ten seconds) and stop the scan from this
  // task: stopping it from inside its own callback stalls the BLE task.
  if (scan.start(10000))
  {
    const uint32_t started = millis();
    while (!targetFound && scan.isScanning() && millis() - started < 10000)
    {
      delay(10);
    }
    scan.stop();
  }
  if (!targetFound)
  {
    Serial.println("HID_SCAN_FAILED");
    return false;
  }

  client = BLE.createClient();
  if (!client || !client.connect(target))
  {
    Serial.println("HID_CONNECT_FAILED");
    return false;
  }
  Serial.println("PEER_CONNECTED");

  // The Central starts pairing itself, as the old forced authentication did.
  // A failure here is not fatal: a security request from the Peripheral can
  // still pair the link, and the wait below decides either way.
  (void)client.secureConnection();
  for (size_t wait = 0; wait < 10000 && !securityComplete; ++wait)
  {
    delay(1);
  }
  if (!securityComplete)
  {
    Serial.println("SECURITY_TIMEOUT");
    return false;
  }

  BLERemoteService hidService = client.getService(HID_SERVICE_UUID);
  if (!hidService)
  {
    Serial.println("HID_SERVICE_MISSING");
    return false;
  }
  BLERemoteCharacteristic reportMap = hidService.getCharacteristic(REPORT_MAP_UUID);
  const String mapValue = !reportMap ? String() : reportMap.readValue();
  bool keyboardMap = false;
  bool mouseMap = false;
  uint8_t mouseButtons = 0;
  for (size_t index = 0; index + 1 < mapValue.length(); ++index)
  {
    if (static_cast<uint8_t>(mapValue[index]) == 0x09 &&
        static_cast<uint8_t>(mapValue[index + 1]) == 0x06)
    {
      keyboardMap = true;
    }
    if (static_cast<uint8_t>(mapValue[index]) == 0x09 &&
        static_cast<uint8_t>(mapValue[index + 1]) == 0x02)
    {
      mouseMap = true;
    }
    if (index + 5 < mapValue.length() &&
        static_cast<uint8_t>(mapValue[index]) == 0x05 &&
        static_cast<uint8_t>(mapValue[index + 1]) == 0x09 &&
        static_cast<uint8_t>(mapValue[index + 2]) == 0x19 &&
        static_cast<uint8_t>(mapValue[index + 3]) == 0x01 &&
        static_cast<uint8_t>(mapValue[index + 4]) == 0x29)
    {
      mouseButtons = static_cast<uint8_t>(mapValue[index + 5]);
    }
  }
  Serial.printf("REPORT_MAP keyboard=%u mouse=%u buttons=%u length=%u\n",
    keyboardMap ? 1 : 0, mouseMap ? 1 : 0, mouseButtons,
    static_cast<unsigned>(mapValue.length()));

  // Discovery order is handle order, as the old handle-keyed map was.
  for (BLERemoteCharacteristic &characteristic : hidService.getCharacteristics())
  {
    if (!(characteristic.getUUID() == REPORT_UUID))
    {
      continue;
    }
    BLERemoteDescriptor reference = characteristic.getDescriptor(REPORT_REFERENCE_UUID);
    const String value = !reference ? String() : reference.readValue();
    if (value.length() != 2)
    {
      continue;
    }
    if (static_cast<uint8_t>(value[0]) == 1 && static_cast<uint8_t>(value[1]) == 1)
    {
      inputReport = characteristic;
    }
    else if (static_cast<uint8_t>(value[0]) == 1 && static_cast<uint8_t>(value[1]) == 2)
    {
      outputReport = characteristic;
    }
    else if (static_cast<uint8_t>(value[0]) == 2 && static_cast<uint8_t>(value[1]) == 1)
    {
      mouseInputReport = characteristic;
    }
  }

  BLERemoteService batteryService = client.getService(BATTERY_SERVICE_UUID);
  BLERemoteCharacteristic battery = !batteryService
    ? BLERemoteCharacteristic()
    : batteryService.getCharacteristic(BATTERY_LEVEL_UUID);
  const uint8_t batteryLevel = !battery ? 0 : battery.readUInt8();
  Serial.printf(
    "HID_REPORTS keyboard=%u mouse=%u output=%u battery=%u\n",
    inputReport ? 1 : 0,
    mouseInputReport ? 1 : 0,
    outputReport ? 1 : 0,
    batteryLevel);
  if (!inputReport || !mouseInputReport || !outputReport || batteryLevel != 87)
  {
    return false;
  }
  const bool subscribed =
    static_cast<bool>(inputReport.subscribe(BLESubscription::Notify, inputNotification));
  const bool mouseSubscribed =
    static_cast<bool>(mouseInputReport.subscribe(BLESubscription::Notify, mouseNotification));
  Serial.printf("INPUT_SUBSCRIBED keyboard=%u mouse=%u\n", subscribed ? 1 : 0, mouseSubscribed ? 1 : 0);
  return subscribed && mouseSubscribed;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  BLE.begin("EspBle HID Peer");
  BLESecurity security = BLE.getSecurity();
  security.setIOCapability(BLEIOCapability::NoInputNoOutput);
  security.setAuthenticationMode(true, false, true);
  security.onAuthenticationComplete(onAuthenticationComplete);

  // RECOVER mirrors the 'd' command and forgets the discovered handles, which
  // belong to the connection that just ended; the next 's' finds them again.
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
  if (Serial.available() > 0)
  {
    const char command = EspBleTestLifecycle::filter(Serial.read());
    // STOP has taken the stack down; the commands have nothing left to act on.
    if (stackStopped) return;
    if (command == 'x')
    {
      clearBonds();
    }
    else if (command == 's')
    {
      Serial.println("HID_SCAN_STARTED");
      Serial.println(connectAndDiscover() ? "HID_READY" : "HID_SETUP_FAILED");
    }
    else if (command == 'l' && outputReport)
    {
      uint8_t leds = 0x03;
      const bool success = static_cast<bool>(outputReport.writeValue(&leds, 1, true));
      Serial.printf("OUTPUT_WRITE success=%u\n", success ? 1 : 0);
    }
    else if (command == 'd' && client)
    {
      client.disconnect();
      Serial.println("PEER_DISCONNECTED");
    }
  }
  delay(1);
}
