// midi_device DUT: a BLE MIDI central implemented with the Arduino-ESP32
// bundled NimBLE API (not EspBle), so it independently validates the exact BLE
// MIDI wire bytes EspBleMidiDevice notifies. It subscribes to the MIDI I/O
// characteristic, captures raw notification bytes, and can write a raw BLE MIDI
// packet back to the peripheral.
#include <Arduino.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>

#if !defined(CONFIG_NIMBLE_ENABLED)
#error "EspBle peer tests require the Arduino-ESP32 NimBLE backend"
#endif

static BLEUUID MIDI_SERVICE_UUID("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
static BLEUUID MIDI_IO_UUID("7772E5DB-3868-4112-A1A9-F2669D106BF3");

BLEAdvertisedDevice *target = nullptr;
BLEClient *client = nullptr;
BLERemoteCharacteristic *ioCharacteristic = nullptr;

volatile uint32_t notifyCount = 0;
uint8_t lastNotify[16] = {0};
volatile size_t lastNotifyLength = 0;

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice device) override
  {
    if (target == nullptr && device.haveServiceUUID() && device.isAdvertisingService(MIDI_SERVICE_UUID))
    {
      target = new BLEAdvertisedDevice(device);
      BLEDevice::getScan()->stop();
    }
  }
};

static void midiNotification(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
  size_t copy = length < sizeof(lastNotify) ? length : sizeof(lastNotify);
  for (size_t i = 0; i < copy; ++i)
    lastNotify[i] = data[i];
  lastNotifyLength = copy;
  ++notifyCount;
}

static bool connectAndSubscribe()
{
  target = nullptr;
  ioCharacteristic = nullptr;
  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), true);
  scan->setActiveScan(true);
  scan->start(10, false);
  if (target == nullptr)
  {
    Serial.println("MIDI_SCAN_FAILED");
    return false;
  }

  client = BLEDevice::createClient();
  if (client == nullptr || !client->connect(target))
  {
    Serial.println("MIDI_CONNECT_FAILED");
    return false;
  }

  BLERemoteService *service = client->getService(MIDI_SERVICE_UUID);
  if (service == nullptr)
  {
    Serial.println("MIDI_SERVICE_MISSING");
    return false;
  }
  ioCharacteristic = service->getCharacteristic(MIDI_IO_UUID);
  if (ioCharacteristic == nullptr)
  {
    Serial.println("MIDI_CHAR_MISSING");
    return false;
  }
  // A BLE MIDI read must return an empty value.
  const String initial = ioCharacteristic->readValue();
  Serial.printf("MIDI_READ length=%u\n", static_cast<unsigned>(initial.length()));
  const bool subscribed = ioCharacteristic->subscribe(true, midiNotification, true);
  return subscribed;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  BLEDevice::init("EspBle MIDI Peer Host");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's')
    {
      Serial.println("MIDI_SCAN_STARTED");
      Serial.println(connectAndSubscribe() ? "MIDI_READY 1" : "MIDI_READY 0");
    }
    else if (command == 'c')
    {
      notifyCount = 0;
      lastNotifyLength = 0;
      Serial.println("MIDI_COUNTERS_RESET");
    }
    else if (command == 'q')
    {
      Serial.printf(
        "NOTIFY count=%u length=%u b0=%u b1=%u b2=%u b3=%u b4=%u\n",
        static_cast<unsigned>(notifyCount),
        static_cast<unsigned>(lastNotifyLength),
        lastNotifyLength > 0 ? lastNotify[0] : 0,
        lastNotifyLength > 1 ? lastNotify[1] : 0,
        lastNotifyLength > 2 ? lastNotify[2] : 0,
        lastNotifyLength > 3 ? lastNotify[3] : 0,
        lastNotifyLength > 4 ? lastNotify[4] : 0);
    }
    else if (command == 'w' && ioCharacteristic != nullptr)
    {
      // Write a raw BLE MIDI packet: timestamp 0, Control Change ch0 ctrl7 val100.
      uint8_t packet[] = {0x80, 0x80, 0xB0, 0x07, 0x64};
      const bool wrote = ioCharacteristic->writeValue(packet, sizeof(packet), false);
      Serial.printf("WRITE_DONE %u\n", wrote ? 1 : 0);
    }
    else if (command == 'd' && client != nullptr)
    {
      client->disconnect();
      Serial.println("PEER_DISCONNECTED");
    }
  }
  delay(1);
}
