// midi_host peer_device: a BLE MIDI peripheral implemented with the bundled
// Arduino-ESP32 BLE API (not EspBle). It notifies a hand-crafted BLE MIDI
// packet that uses running status so the EspBle MIDI Host DUT is validated
// against an independent sender, and it captures host writes.
#include <Arduino.h>
#include <BLE.h>

#include "../../../sketch_support/EspBleTestLifecycle.h"

#if !defined(CONFIG_NIMBLE_ENABLED)
#error "EspBle peer tests require the Arduino-ESP32 NimBLE backend"
#endif

static const BLEUUID MIDI_SERVICE_UUID("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
static const BLEUUID MIDI_IO_UUID("7772E5DB-3868-4112-A1A9-F2669D106BF3");

BLEServer server;
BLECharacteristic midiCharacteristic;
bool advertising = false;
bool stackStopped = false;

uint8_t hostIn[16] = {0};
size_t hostInLength = 0;
uint32_t hostInCount = 0;

// Independent BLE MIDI SysEx reassembler (does not use EspBle).
uint8_t sysexBuffer[512] = {0};
size_t sysexLength = 0;
bool sysexInProgress = false;
bool sysexComplete = false;

static void feedSysEx(const uint8_t *data, size_t length)
{
  size_t i = 1; // skip the packet header byte
  while (i < length)
  {
    const uint8_t b = data[i];
    if (sysexInProgress)
    {
      if (b & 0x80)
      {
        if (i + 1 >= length)
          break;
        const uint8_t status = data[i + 1];
        i += 2;
        if (status == 0xF7)
        {
          sysexInProgress = false;
          sysexComplete = true;
        }
      }
      else
      {
        if (sysexLength < sizeof(sysexBuffer))
          sysexBuffer[sysexLength++] = b;
        ++i;
      }
    }
    else
    {
      if (b & 0x80)
      {
        if (i + 1 >= length)
          break;
        const uint8_t status = data[i + 1];
        i += 2;
        if (status == 0xF0)
        {
          sysexInProgress = true;
          sysexComplete = false;
          sysexLength = 0;
        }
      }
      else
      {
        ++i;
      }
    }
  }
}

// Runs on the BLE stack task.
static void onMidiWrite(BLECharacteristic &characteristic, const BLEConnInfo &)
{
  size_t length = 0;
  const uint8_t *value = characteristic.getValue(&length);
  if (value == nullptr)
  {
    length = 0;
  }
  hostInLength = length < sizeof(hostIn) ? length : sizeof(hostIn);
  for (size_t i = 0; i < hostInLength; ++i)
    hostIn[i] = value[i];
  ++hostInCount;
  static uint8_t packet[512];
  const size_t copy = length < sizeof(packet) ? length : sizeof(packet);
  for (size_t i = 0; i < copy; ++i)
    packet[i] = value[i];
  feedSysEx(packet, copy);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLE.begin("EspBle MIDI Peer"))
  {
    Serial.println("PERIPHERAL_INIT_FAILED");
    return;
  }

  server = BLE.createServer();
  server.onConnect([](BLEServer &, const BLEConnInfo &) { advertising = false; });
  // The server advertises again by itself once a link drops.
  server.advertiseOnDisconnect(true);
  server.onDisconnect([](BLEServer &, const BLEConnInfo &, uint8_t) { advertising = true; });
  BLEService service = server.createService(MIDI_SERVICE_UUID);
  midiCharacteristic = service.createCharacteristic(
    MIDI_IO_UUID,
    BLEProperty::Read | BLEProperty::WriteNR | BLEProperty::Notify,
    BLEPermission::ReadWriteOpen);
  midiCharacteristic.onWrite(onMidiWrite);
  midiCharacteristic.setValue(String()); // BLE MIDI read is empty
  if (!server.start())
  {
    Serial.println("PERIPHERAL_INIT_FAILED");
    return;
  }

  BLEAdvertising advertisingHandle = BLE.getAdvertising();
  advertisingHandle.addServiceUUID(MIDI_SERVICE_UUID);
  advertisingHandle.setScanResponse(true);
  if (!BLE.startAdvertising())
  {
    Serial.println("PERIPHERAL_INIT_FAILED");
    return;
  }
  advertising = true;
  Serial.println("PERIPHERAL_READY");

  // The sketch tracks no connection id; the wrapper's own peer map has them.
  // onDisconnect advertises again, and the predicate covers the case where it
  // did not. end() takes the radio down synchronously, so STOP is complete
  // at once.
  EspBleTestLifecycle::Hooks hooks;
  hooks.stop = []() {
    stackStopped = true;
    advertising = false;
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
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", advertising ? 1 : 0);
    }
    else if (command == 'm' && midiCharacteristic)
    {
      // Two Note On messages in one packet using running status. Header 0x81
      // -> timestampHigh=1; 0xA5 -> ts=165; second message (0xA6 -> ts=166)
      // omits the status byte and reuses 0x90.
      uint8_t packet[] = {0x81, 0xA5, 0x90, 0x3C, 0x40, 0xA6, 0x40, 0x40};
      midiCharacteristic.setValue(packet, sizeof(packet));
      midiCharacteristic.notify();
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
    else if (command == 'c')
    {
      hostInCount = 0;
      sysexLength = 0;
      sysexInProgress = false;
      sysexComplete = false;
      Serial.println("PEER_RESET");
    }
    else if (command == 'g')
    {
      uint32_t sum = 0;
      for (size_t i = 0; i < sysexLength; ++i)
        sum += sysexBuffer[i];
      Serial.printf("SYSEX complete=%u length=%u first=%u last=%u sum=%u\n",
        sysexComplete ? 1 : 0, static_cast<unsigned>(sysexLength),
        sysexLength > 0 ? sysexBuffer[0] : 0,
        sysexLength > 0 ? sysexBuffer[sysexLength - 1] : 0,
        static_cast<unsigned>(sum));
    }
  }
  delay(1);
}
