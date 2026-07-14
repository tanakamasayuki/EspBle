#include <Arduino.h>
#include <BLEDevice.h>

#if !defined(CONFIG_NIMBLE_ENABLED)
#error "EspBle peer tests require the Arduino-ESP32 NimBLE backend"
#endif

static BLEUUID serviceUuid("8d47a620-8d3a-4d65-a76f-6f7370626c65");
static BLEUUID characteristicUuid("8d47a621-8d3a-4d65-a76f-6f7370626c65");
static BLEAdvertisedDevice *peer = nullptr;
static bool connectPending = false;
static bool complete = false;

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice device) override
  {
    if (!device.haveServiceUUID() || !device.isAdvertisingService(serviceUuid))
    {
      return;
    }

    BLEDevice::getScan()->stop();
    peer = new BLEAdvertisedDevice(device);
    connectPending = true;
    Serial.println("SCAN_FOUND");
  }
};

static bool connectAndExerciseGatt()
{
  BLEClient *client = BLEDevice::createClient();
  if (client == nullptr || !client->connect(peer))
  {
    return false;
  }
  Serial.println("CONNECTED");

  BLERemoteService *service = client->getService(serviceUuid);
  if (service == nullptr)
  {
    return false;
  }

  BLERemoteCharacteristic *characteristic = service->getCharacteristic(characteristicUuid);
  if (characteristic == nullptr)
  {
    return false;
  }

  String value = characteristic->readValue();
  Serial.printf("READ %s\n", value.c_str());
  char payload[] = "central-write";
  const bool wrote = characteristic->writeValue(
    reinterpret_cast<uint8_t *>(payload), sizeof(payload) - 1, true);
  Serial.printf("WRITE %u\n", wrote ? 1 : 0);
  return wrote;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLEDevice::init(""))
  {
    Serial.println("CENTRAL_INIT_FAILED");
    return;
  }

  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
  scan->setActiveScan(true);
  Serial.println("CENTRAL_READY");
  scan->start(5, false);
}

void loop()
{
  if (connectPending && !complete)
  {
    connectPending = false;
    complete = connectAndExerciseGatt();
    if (!complete)
    {
      Serial.println("CONNECT_OR_GATT_FAILED");
    }
  }
  else if (!complete && !connectPending)
  {
    BLEDevice::getScan()->start(5, false);
  }
  delay(10);
}
