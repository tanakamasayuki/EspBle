// midi_host peer_device: a BLE MIDI peripheral implemented with the bundled
// Arduino-ESP32 NimBLE API (not EspBle). It notifies a hand-crafted BLE MIDI
// packet that uses running status so the EspBle MIDI Host DUT is validated
// against an independent sender, and it captures host writes.
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#if !defined(CONFIG_NIMBLE_ENABLED)
#error "EspBle peer tests require the Arduino-ESP32 NimBLE backend"
#endif

static constexpr const char *MIDI_SERVICE_UUID = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
static constexpr const char *MIDI_IO_UUID = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

BLECharacteristic *midiCharacteristic = nullptr;
bool advertising = false;

uint8_t hostIn[16] = {0};
size_t hostInLength = 0;
uint32_t hostInCount = 0;

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *) override { advertising = false; }
  void onDisconnect(BLEServer *) override
  {
    BLEDevice::startAdvertising();
    advertising = true;
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic) override
  {
    String value = characteristic->getValue();
    hostInLength = value.length() < sizeof(hostIn) ? value.length() : sizeof(hostIn);
    for (size_t i = 0; i < hostInLength; ++i)
      hostIn[i] = static_cast<uint8_t>(value[i]);
    ++hostInCount;
  }
};

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLEDevice::init("EspBle MIDI Peer"))
  {
    Serial.println("PERIPHERAL_INIT_FAILED");
    return;
  }

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService *service = server->createService(MIDI_SERVICE_UUID);
  midiCharacteristic = service->createCharacteristic(
    MIDI_IO_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE_NR |
      BLECharacteristic::PROPERTY_NOTIFY);
  midiCharacteristic->setCallbacks(new CharacteristicCallbacks());
  midiCharacteristic->setValue((uint8_t *)nullptr, 0); // BLE MIDI read is empty
  service->start();

  BLEAdvertising *advertisingHandle = BLEDevice::getAdvertising();
  advertisingHandle->addServiceUUID(MIDI_SERVICE_UUID);
  advertisingHandle->setScanResponse(true);
  BLEDevice::startAdvertising();
  advertising = true;
  Serial.println("PERIPHERAL_READY");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", advertising ? 1 : 0);
    }
    else if (command == 'm' && midiCharacteristic != nullptr)
    {
      // Two Note On messages in one packet using running status. Header 0x81
      // -> timestampHigh=1; 0xA5 -> ts=165; second message (0xA6 -> ts=166)
      // omits the status byte and reuses 0x90.
      uint8_t packet[] = {0x81, 0xA5, 0x90, 0x3C, 0x40, 0xA6, 0x40, 0x40};
      midiCharacteristic->setValue(packet, sizeof(packet));
      midiCharacteristic->notify();
      Serial.println("NOTIFIED");
    }
    else if (command == 'q')
    {
      Serial.printf("HOST_IN count=%u length=%u b2=%u b3=%u b4=%u\n",
        static_cast<unsigned>(hostInCount),
        static_cast<unsigned>(hostInLength),
        hostInLength > 2 ? hostIn[2] : 0,
        hostInLength > 3 ? hostIn[3] : 0,
        hostInLength > 4 ? hostIn[4] : 0);
    }
  }
  delay(1);
}
