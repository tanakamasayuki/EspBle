#include <Arduino.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLESecurity.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteDescriptor.h>
#include <BLERemoteService.h>
#include <host/ble_store.h>

static BLEUUID HID_SERVICE_UUID((uint16_t)0x1812);
static BLEUUID REPORT_UUID((uint16_t)0x2a4d);
static BLEUUID REPORT_MAP_UUID((uint16_t)0x2a4b);
static BLEUUID REPORT_REFERENCE_UUID((uint16_t)0x2908);
static BLEUUID BATTERY_SERVICE_UUID((uint16_t)0x180f);
static BLEUUID BATTERY_LEVEL_UUID((uint16_t)0x2a19);

BLEAdvertisedDevice *target = nullptr;
BLEClient *client = nullptr;
BLERemoteCharacteristic *inputReport = nullptr;
BLERemoteCharacteristic *outputReport = nullptr;
volatile bool securityComplete = false;

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice device) override
  {
    if (target == nullptr && device.haveServiceUUID() && device.isAdvertisingService(HID_SERVICE_UUID))
    {
      target = new BLEAdvertisedDevice(device);
      BLEDevice::getScan()->stop();
    }
  }
};

class SecurityCallbacks : public BLESecurityCallbacks
{
  void onAuthenticationComplete(ble_gap_conn_desc *description) override
  {
    securityComplete = description != nullptr && description->sec_state.encrypted;
    if (description != nullptr)
    {
      Serial.printf("PEER_SECURITY encrypted=%u bonded=%u\n",
        description->sec_state.encrypted, description->sec_state.bonded);
    }
  }
};

static void clearBonds()
{
  ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
  int count = 0;
  ble_store_util_bonded_peers(peers, &count, CONFIG_BT_NIMBLE_MAX_BONDS);
  for (int index = 0; index < count; ++index)
  {
    ble_store_util_delete_peer(&peers[index]);
  }
  count = 0;
  ble_store_util_bonded_peers(peers, &count, CONFIG_BT_NIMBLE_MAX_BONDS);
  Serial.printf("PEER_BONDS_CLEARED count=%d\n", count);
}

static void inputNotification(
  BLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
  Serial.printf(
    "INPUT_REPORT length=%u modifiers=%u key0=%u\n",
    static_cast<unsigned>(length),
    length > 0 ? data[0] : 0,
    length > 2 ? data[2] : 0);
}

static bool connectAndDiscover()
{
  target = nullptr;
  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), true);
  scan->setActiveScan(true);
  scan->start(10, false);
  if (target == nullptr)
  {
    Serial.println("HID_SCAN_FAILED");
    return false;
  }

  client = BLEDevice::createClient();
  if (client == nullptr || !client->connect(target))
  {
    Serial.println("HID_CONNECT_FAILED");
    return false;
  }
  Serial.println("PEER_CONNECTED");

  for (size_t wait = 0; wait < 10000 && !securityComplete; ++wait)
  {
    delay(1);
  }
  if (!securityComplete)
  {
    Serial.println("SECURITY_TIMEOUT");
    return false;
  }

  BLERemoteService *hidService = client->getService(HID_SERVICE_UUID);
  if (hidService == nullptr)
  {
    Serial.println("HID_SERVICE_MISSING");
    return false;
  }
  BLERemoteCharacteristic *reportMap = hidService->getCharacteristic(REPORT_MAP_UUID);
  const String mapValue = reportMap == nullptr ? String() : reportMap->readValue();
  bool keyboardMap = false;
  for (size_t index = 0; index + 1 < mapValue.length(); ++index)
  {
    if (static_cast<uint8_t>(mapValue[index]) == 0x09 &&
        static_cast<uint8_t>(mapValue[index + 1]) == 0x06)
    {
      keyboardMap = true;
      break;
    }
  }
  Serial.printf("REPORT_MAP valid=%u length=%u\n",
    keyboardMap ? 1 : 0, static_cast<unsigned>(mapValue.length()));

  std::map<uint16_t, BLERemoteCharacteristic *> *characteristics =
    hidService->getCharacteristicsByHandle();
  for (const auto &entry : *characteristics)
  {
    BLERemoteCharacteristic *characteristic = entry.second;
    if (!characteristic->getUUID().equals(REPORT_UUID))
    {
      continue;
    }
    BLERemoteDescriptor *reference = characteristic->getDescriptor(REPORT_REFERENCE_UUID);
    const String value = reference == nullptr ? String() : reference->readValue();
    if (value.length() != 2 || static_cast<uint8_t>(value[0]) != 1)
    {
      continue;
    }
    if (static_cast<uint8_t>(value[1]) == 1)
    {
      inputReport = characteristic;
    }
    else if (static_cast<uint8_t>(value[1]) == 2)
    {
      outputReport = characteristic;
    }
  }

  BLERemoteService *batteryService = client->getService(BATTERY_SERVICE_UUID);
  BLERemoteCharacteristic *battery = batteryService == nullptr
    ? nullptr
    : batteryService->getCharacteristic(BATTERY_LEVEL_UUID);
  const uint8_t batteryLevel = battery == nullptr ? 0 : battery->readUInt8();
  Serial.printf(
    "HID_REPORTS input=%u output=%u battery=%u\n",
    inputReport != nullptr ? 1 : 0,
    outputReport != nullptr ? 1 : 0,
    batteryLevel);
  if (inputReport == nullptr || outputReport == nullptr || batteryLevel != 87)
  {
    return false;
  }
  const bool subscribed = inputReport->subscribe(true, inputNotification, true);
  Serial.printf("INPUT_SUBSCRIBED success=%u\n", subscribed ? 1 : 0);
  return subscribed;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  BLEDevice::init("EspBle HID Peer");
  static BLESecurity security;
  security.setCapability(ESP_IO_CAP_NONE);
  security.setAuthenticationMode(true, false, true);
  security.setForceAuthentication(true);
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      clearBonds();
    }
    else if (command == 's')
    {
      Serial.println("HID_SCAN_STARTED");
      Serial.println(connectAndDiscover() ? "HID_READY" : "HID_SETUP_FAILED");
    }
    else if (command == 'l' && outputReport != nullptr)
    {
      uint8_t leds = 0x03;
      const bool success = outputReport->writeValue(&leds, 1, true);
      Serial.printf("OUTPUT_WRITE success=%u\n", success ? 1 : 0);
    }
    else if (command == 'd' && client != nullptr)
    {
      client->disconnect();
      Serial.println("PEER_DISCONNECTED");
    }
  }
  delay(1);
}
