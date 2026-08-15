// Peer for the core-host BLE MIDI interoperability test. It links no EspBle
// code: the MIDI service is assembled by hand from the BLE wrapper
// Arduino-ESP32 ships, so the packets the DUT decodes were framed by another
// implementation rather than by EspBle's own encoder.
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// The MIDI service and its single I/O characteristic, as defined by the
// MIDI over Bluetooth Low Energy specification.
static const char *MidiServiceUuid = "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
static const char *MidiIoUuid = "7772e5db-3868-4112-a1a9-f2669d106bf3";

BLEServer *server = nullptr;
BLECharacteristic *io = nullptr;
bool linkUp = false;
uint16_t connectionId = 0;
uint16_t subscription = 0;
unsigned received = 0;
String lastReceivedHex;

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
  Serial.printf("MIDIPEER_READY address=%s\n",
    BLEDevice::getAddress().toString().c_str());
  Serial.printf("MIDIPEER_STATE connected=%u cccd=%04x received=%u last=%s\n",
    linkUp ? 1 : 0, subscription, received,
    lastReceivedHex.length() != 0 ? lastReceivedHex.c_str() : "none");
}

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *serverInstance, esp_ble_gatts_cb_param_t *param) override
  {
    linkUp = true;
    connectionId = param->connect.conn_id;
    Serial.printf("MIDIPEER_CONNECTED id=%u\n", connectionId);
  }

  void onDisconnect(BLEServer *serverInstance) override
  {
    linkUp = false;
    subscription = 0;
    Serial.println("MIDIPEER_DISCONNECTED");
    BLEDevice::startAdvertising();
  }
};

class IoCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic) override
  {
    const String value = characteristic->getValue();
    ++received;
    lastReceivedHex =
      toHex(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
    // Printed raw: the header and timestamp bytes EspBle produced are part of
    // what this test checks, not just the MIDI status and data bytes.
    Serial.printf("MIDIPEER_RX count=%u length=%u hex=%s\n", received,
      static_cast<unsigned>(value.length()), lastReceivedHex.c_str());
  }
};

class CccdCallbacks : public BLEDescriptorCallbacks
{
  void onWrite(BLEDescriptor *descriptor) override
  {
    const uint8_t *value = descriptor->getValue();
    subscription = descriptor->getLength() >= 2
                     ? static_cast<uint16_t>(value[0] | (value[1] << 8))
                     : 0;
    Serial.printf("MIDIPEER_CCCD value=%04x\n", subscription);
  }
};

void sendPacket(const uint8_t *packet, size_t length)
{
  io->setValue(const_cast<uint8_t *>(packet), length);
  io->notify();
  Serial.printf("MIDIPEER_TX hex=%s\n", toHex(packet, length).c_str());
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  BLEDevice::init("EspBle CoreHost MIDI");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(BLEUUID(MidiServiceUuid), 20);
  io = service->createCharacteristic(
    BLEUUID(MidiIoUuid),
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE_NR
      | BLECharacteristic::PROPERTY_NOTIFY);
  io->setCallbacks(new IoCallbacks());
  BLE2902 *cccd = new BLE2902();
  cccd->setCallbacks(new CccdCallbacks());
  io->addDescriptor(cccd);
  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLEUUID(MidiServiceUuid));
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
      // Header 0x80 | timestampHigh, then timestamp low, then Note On.
      const uint8_t packet[] = {0x80, 0xa1, 0x90, 0x3c, 0x64};
      sendPacket(packet, sizeof(packet));
    }
    else if (command == "r")
    {
      // Running status: the second note carries no status byte of its own, and
      // a decoder that does not track running status drops or mangles it.
      const uint8_t packet[] = {0x80, 0xa2, 0x90, 0x3c, 0x64, 0xa3, 0x40, 0x50};
      sendPacket(packet, sizeof(packet));
    }
    else if (command == "s")
    {
      // System Exclusive split across one packet, framed the way the spec
      // requires: F0 ... F7 with a timestamp byte before the terminator.
      const uint8_t packet[] = {0x80, 0xa4, 0xf0, 0x7d, 0x01, 0x02, 0x03, 0xa5, 0xf7};
      sendPacket(packet, sizeof(packet));
    }
    else if (command == "d" && linkUp)
    {
      server->disconnect(connectionId);
      Serial.println("MIDIPEER_DISCONNECT requested=1");
    }
  }
  delay(10);
}
