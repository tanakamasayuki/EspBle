// Peer for the core-host BLE MIDI interoperability test. It links no EspBle
// code: the MIDI service is assembled by hand from the BLE wrapper
// Arduino-ESP32 ships, so the packets the DUT decodes were framed by another
// implementation rather than by EspBle's own encoder.
#include <BLE.h>

#include "../../../sketch_support/EspBleTestLifecycle.h"

// The MIDI service and its single I/O characteristic, as defined by the
// MIDI over Bluetooth Low Energy specification.
static const BLEUUID MidiServiceUuid("03b80e5a-ede8-4b33-a751-6ce34ec4c700");
static const BLEUUID MidiIoUuid("7772e5db-3868-4112-a1a9-f2669d106bf3");

BLEServer server;
BLECharacteristic io;
bool linkUp = false;
uint16_t connectionId = 0;
uint16_t subscription = 0;
unsigned received = 0;
String lastReceivedHex;
bool stackStopped = false;

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
    BLE.getAddress().toString().c_str());
  Serial.printf("MIDIPEER_STATE connected=%u cccd=%04x received=%u last=%s\n",
    linkUp ? 1 : 0, subscription, received,
    lastReceivedHex.length() != 0 ? lastReceivedHex.c_str() : "none");
}

// The handlers below run on the BLE stack task.
static void onConnect(BLEServer &, const BLEConnInfo &connection)
{
  linkUp = true;
  connectionId = connection.getHandle();
  Serial.printf("MIDIPEER_CONNECTED id=%u\n", connectionId);
}

static void onDisconnect(BLEServer &, const BLEConnInfo &, uint8_t)
{
  linkUp = false;
  subscription = 0;
  Serial.println("MIDIPEER_DISCONNECTED");
  // The server advertises again by itself (advertiseOnDisconnect).
}

static void onIoWrite(BLECharacteristic &characteristic, const BLEConnInfo &)
{
  size_t length = 0;
  const uint8_t *value = characteristic.getValue(&length);
  if (value == nullptr)
  {
    length = 0;
  }
  ++received;
  lastReceivedHex = toHex(value, length);
  // Printed raw: the header and timestamp bytes EspBle produced are part of
  // what this test checks, not just the MIDI status and data bytes.
  Serial.printf("MIDIPEER_RX count=%u length=%u hex=%s\n", received,
    static_cast<unsigned>(length), lastReceivedHex.c_str());
}

// The library owns the CCCD and reports each change of its value here.
static void onIoSubscribe(BLECharacteristic &, const BLEConnInfo &, uint16_t bits)
{
  subscription = bits;
  Serial.printf("MIDIPEER_CCCD value=%04x\n", subscription);
}

void sendPacket(const uint8_t *packet, size_t length)
{
  io.setValue(packet, length);
  io.notify();
  Serial.printf("MIDIPEER_TX hex=%s\n", toHex(packet, length).c_str());
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLE.begin("EspBle CoreHost MIDI"))
  {
    Serial.println("MIDIPEER_INIT_FAILED");
    return;
  }
  server = BLE.createServer();
  server.onConnect(onConnect);
  server.onDisconnect(onDisconnect);
  server.advertiseOnDisconnect(true);

  BLEService service = server.createService(MidiServiceUuid, 20);
  // The CCCD is created with the characteristic.
  io = service.createCharacteristic(
    MidiIoUuid,
    BLEProperty::Read | BLEProperty::WriteNR | BLEProperty::Notify,
    BLEPermission::ReadWriteOpen);
  io.onWrite(onIoWrite);
  io.onSubscribe(onIoSubscribe);
  if (!server.start())
  {
    Serial.println("MIDIPEER_INIT_FAILED");
    return;
  }

  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.addServiceUUID(MidiServiceUuid);
  advertising.setScanResponse(true);
  if (!BLE.startAdvertising())
  {
    Serial.println("MIDIPEER_INIT_FAILED");
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
      server.disconnect(connectionId);
      Serial.println("MIDIPEER_DISCONNECT requested=1");
    }
  }
  delay(10);
}
