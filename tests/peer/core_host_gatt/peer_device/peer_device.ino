// Peer for the core-host GATT interoperability test. It links no EspBle code:
// everything here is the BLE wrapper Arduino-ESP32 ships, which on the original
// ESP32 runs on Bluedroid. The DUT runs EspBle on a NimBLE host, so every
// exchange in this suite crosses a stack boundary.
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

static const char *ServiceUuid = "2f7a1000-9d0b-4f6a-9b41-1c8f3a5d0001";
static const char *DataUuid = "2f7a1001-9d0b-4f6a-9b41-1c8f3a5d0001";
static const char *NotifyUuid = "2f7a1002-9d0b-4f6a-9b41-1c8f3a5d0001";
static const char *IndicateUuid = "2f7a1003-9d0b-4f6a-9b41-1c8f3a5d0001";

BLEServer *server = nullptr;
BLECharacteristic *dataCharacteristic = nullptr;
BLECharacteristic *notifyCharacteristic = nullptr;
BLECharacteristic *indicateCharacteristic = nullptr;
bool linkUp = false;
uint16_t connectionId = 0;
uint16_t notifySubscribers = 0;
uint16_t indicateSubscribers = 0;
uint32_t notifyCounter = 0;

String toHex(const uint8_t *data, size_t length)
{
  String text;
  char octet[3];
  for (size_t index = 0; index < length; ++index)
  {
    snprintf(octet, sizeof(octet), "%02x", data[index]);
    text += octet;
  }
  return text;
}

void reportReady()
{
  // The address and the readiness line are printed at boot and repeated on "?",
  // so the test can ask instead of racing the flash of the other board.
  Serial.printf("COREPEER_READY address=%s\n",
    BLEDevice::getAddress().toString().c_str());
  Serial.printf("COREPEER_LINK connected=%u mtu=%u notify_cccd=%u indicate_cccd=%u\n",
    linkUp ? 1 : 0,
    linkUp && server != nullptr ? server->getPeerMTU(connectionId) : 0,
    notifySubscribers, indicateSubscribers);
}

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *serverInstance, esp_ble_gatts_cb_param_t *param) override
  {
    linkUp = true;
    connectionId = param->connect.conn_id;
    Serial.printf("COREPEER_CONNECTED id=%u\n", connectionId);
  }

  void onDisconnect(BLEServer *serverInstance) override
  {
    linkUp = false;
    notifySubscribers = 0;
    indicateSubscribers = 0;
    Serial.println("COREPEER_DISCONNECTED");
    // Bluedroid stops advertising on disconnect; restart so the DUT can
    // reconnect within the same test.
    BLEDevice::startAdvertising();
  }

  void onMtuChanged(BLEServer *serverInstance, esp_ble_gatts_cb_param_t *param) override
  {
    Serial.printf("COREPEER_MTU value=%u\n", param->mtu.mtu);
  }
};

class DataCallbacks : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *characteristic) override
  {
    Serial.println("COREPEER_READ_SERVED");
  }

  void onWrite(BLECharacteristic *characteristic) override
  {
    const String value = characteristic->getValue();
    // Printed as hex because the DUT writes a payload containing 0x00: a stack
    // that treats the value as a C string truncates it here.
    Serial.printf("COREPEER_WRITTEN length=%u hex=%s\n",
      static_cast<unsigned>(value.length()),
      toHex(reinterpret_cast<const uint8_t *>(value.c_str()), value.length()).c_str());
  }
};

class SubscriptionCallbacks : public BLEDescriptorCallbacks
{
public:
  SubscriptionCallbacks(const char *label, uint16_t *counter)
    : label_(label), counter_(counter) {}

  void onWrite(BLEDescriptor *descriptor) override
  {
    const uint8_t *value = descriptor->getValue();
    const uint16_t bits = descriptor->getLength() >= 2
                            ? static_cast<uint16_t>(value[0] | (value[1] << 8))
                            : 0;
    *counter_ = bits;
    Serial.printf("COREPEER_CCCD name=%s value=%04x\n", label_, bits);
  }

private:
  const char *label_;
  uint16_t *counter_;
};

void setup()
{
  Serial.begin(115200);
  delay(500);

  BLEDevice::init("EspBle CoreHost GATT");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(BLEUUID(ServiceUuid), 30);

  dataCharacteristic = service->createCharacteristic(
    BLEUUID(DataUuid),
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
      | BLECharacteristic::PROPERTY_WRITE_NR);
  dataCharacteristic->setCallbacks(new DataCallbacks());
  dataCharacteristic->setValue("core-host-value");

  notifyCharacteristic =
    service->createCharacteristic(BLEUUID(NotifyUuid), BLECharacteristic::PROPERTY_NOTIFY);
  BLE2902 *notifyCccd = new BLE2902();
  notifyCccd->setCallbacks(new SubscriptionCallbacks("notify", &notifySubscribers));
  notifyCharacteristic->addDescriptor(notifyCccd);

  indicateCharacteristic =
    service->createCharacteristic(BLEUUID(IndicateUuid), BLECharacteristic::PROPERTY_INDICATE);
  BLE2902 *indicateCccd = new BLE2902();
  indicateCccd->setCallbacks(new SubscriptionCallbacks("indicate", &indicateSubscribers));
  indicateCharacteristic->addDescriptor(indicateCccd);

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLEUUID(ServiceUuid));
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
    else if (command == "n")
    {
      // Payload carries a zero byte for the same reason as the write above.
      uint8_t payload[4] = {0xa5, 0x00, 0x5a, static_cast<uint8_t>(++notifyCounter)};
      notifyCharacteristic->setValue(payload, sizeof(payload));
      notifyCharacteristic->notify();
      Serial.printf("COREPEER_NOTIFIED hex=%s\n", toHex(payload, sizeof(payload)).c_str());
    }
    else if (command == "i")
    {
      uint8_t payload[3] = {0x0f, 0x00, 0xf0};
      indicateCharacteristic->setValue(payload, sizeof(payload));
      indicateCharacteristic->indicate();
      Serial.printf("COREPEER_INDICATED hex=%s\n", toHex(payload, sizeof(payload)).c_str());
    }
    else if (command == "v")
    {
      dataCharacteristic->setValue("core-host-second");
      Serial.println("COREPEER_VALUE_SET core-host-second");
    }
    else if (command == "d" && linkUp)
    {
      server->disconnect(connectionId);
      Serial.println("COREPEER_DISCONNECT requested=1");
    }
  }
  delay(10);
}
