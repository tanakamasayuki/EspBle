// Peer for the core-host GATT interoperability test. It links no EspBle code:
// everything here is the BLE wrapper Arduino-ESP32 ships, which on the original
// ESP32 runs on Bluedroid. The DUT runs EspBle on a NimBLE host, so every
// exchange in this suite crosses a stack boundary.
#include <BLE.h>

#include "../../../sketch_support/EspBleTestLifecycle.h"

static const BLEUUID ServiceUuid("2f7a1000-9d0b-4f6a-9b41-1c8f3a5d0001");
static const BLEUUID DataUuid("2f7a1001-9d0b-4f6a-9b41-1c8f3a5d0001");
static const BLEUUID NotifyUuid("2f7a1002-9d0b-4f6a-9b41-1c8f3a5d0001");
static const BLEUUID IndicateUuid("2f7a1003-9d0b-4f6a-9b41-1c8f3a5d0001");

BLEServer server;
BLECharacteristic dataCharacteristic;
BLECharacteristic notifyCharacteristic;
BLECharacteristic indicateCharacteristic;
bool linkUp = false;
// Set by onDisconnect, printed from loop(): server.disconnect() can deliver
// the callback before it returns, and the test reads the request line first.
volatile bool disconnectedPending = false;
uint16_t connectionId = 0;
uint16_t notifySubscribers = 0;
uint16_t indicateSubscribers = 0;
uint32_t notifyCounter = 0;
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
  // The address and the readiness line are printed at boot and repeated on "?",
  // so the test can ask instead of racing the flash of the other board.
  Serial.printf("COREPEER_READY address=%s\n",
    BLE.getAddress().toString().c_str());
  Serial.printf("COREPEER_LINK connected=%u mtu=%u notify_cccd=%u indicate_cccd=%u\n",
    linkUp ? 1 : 0,
    linkUp && server ? server.getPeerMTU(connectionId) : 0,
    notifySubscribers, indicateSubscribers);
}

// The handlers below run on the BLE stack task.
static void onConnect(BLEServer &, const BLEConnInfo &connection)
{
  linkUp = true;
  connectionId = connection.getHandle();
  Serial.printf("COREPEER_CONNECTED id=%u\n", connectionId);
}

static void onDisconnect(BLEServer &, const BLEConnInfo &, uint8_t)
{
  linkUp = false;
  notifySubscribers = 0;
  indicateSubscribers = 0;
  disconnectedPending = true;
  // The server advertises again by itself (advertiseOnDisconnect), so the DUT
  // can reconnect within the same test.
}

static void onMtuChanged(BLEServer &, const BLEConnInfo &, uint16_t mtu)
{
  Serial.printf("COREPEER_MTU value=%u\n", mtu);
}

static void onDataRead(BLECharacteristic &, const BLEConnInfo &)
{
  Serial.println("COREPEER_READ_SERVED");
}

static void onDataWrite(BLECharacteristic &characteristic, const BLEConnInfo &)
{
  size_t length = 0;
  const uint8_t *value = characteristic.getValue(&length);
  // Printed as hex because the DUT writes a payload containing 0x00: a stack
  // that treats the value as a C string truncates it here.
  Serial.printf("COREPEER_WRITTEN length=%u hex=%s\n",
    static_cast<unsigned>(length),
    toHex(value, value == nullptr ? 0 : length).c_str());
}

// The library owns the CCCD and reports each change of its value here.
static void onNotifySubscribe(BLECharacteristic &, const BLEConnInfo &, uint16_t bits)
{
  notifySubscribers = bits;
  Serial.printf("COREPEER_CCCD name=%s value=%04x\n", "notify", bits);
}

static void onIndicateSubscribe(BLECharacteristic &, const BLEConnInfo &, uint16_t bits)
{
  indicateSubscribers = bits;
  Serial.printf("COREPEER_CCCD name=%s value=%04x\n", "indicate", bits);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLE.begin("EspBle CoreHost GATT"))
  {
    Serial.println("COREPEER_INIT_FAILED");
    return;
  }
  server = BLE.createServer();
  server.onConnect(onConnect);
  server.onDisconnect(onDisconnect);
  server.onMtuChanged(onMtuChanged);
  server.advertiseOnDisconnect(true);

  BLEService service = server.createService(ServiceUuid, 30);

  dataCharacteristic = service.createCharacteristic(
    DataUuid,
    BLEProperty::Read | BLEProperty::Write | BLEProperty::WriteNR,
    BLEPermission::ReadWriteOpen);
  dataCharacteristic.onRead(onDataRead);
  dataCharacteristic.onWrite(onDataWrite);
  dataCharacteristic.setValue("core-host-value");

  // The CCCDs are created with the characteristics.
  notifyCharacteristic =
    service.createCharacteristic(NotifyUuid, BLEProperty::Notify, BLEPermission::None);
  notifyCharacteristic.onSubscribe(onNotifySubscribe);

  indicateCharacteristic =
    service.createCharacteristic(IndicateUuid, BLEProperty::Indicate, BLEPermission::None);
  indicateCharacteristic.onSubscribe(onIndicateSubscribe);

  if (!server.start())
  {
    Serial.println("COREPEER_INIT_FAILED");
    return;
  }

  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.addServiceUUID(ServiceUuid);
  advertising.setScanResponse(true);
  if (!BLE.startAdvertising())
  {
    Serial.println("COREPEER_INIT_FAILED");
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
  if (disconnectedPending)
  {
    disconnectedPending = false;
    Serial.println("COREPEER_DISCONNECTED");
  }
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
      // Payload carries a zero byte for the same reason as the write above.
      uint8_t payload[4] = {0xa5, 0x00, 0x5a, static_cast<uint8_t>(++notifyCounter)};
      notifyCharacteristic.setValue(payload, sizeof(payload));
      notifyCharacteristic.notify();
      Serial.printf("COREPEER_NOTIFIED hex=%s\n", toHex(payload, sizeof(payload)).c_str());
    }
    else if (command == "i")
    {
      uint8_t payload[3] = {0x0f, 0x00, 0xf0};
      indicateCharacteristic.setValue(payload, sizeof(payload));
      indicateCharacteristic.indicate();
      Serial.printf("COREPEER_INDICATED hex=%s\n", toHex(payload, sizeof(payload)).c_str());
    }
    else if (command == "v")
    {
      dataCharacteristic.setValue("core-host-second");
      Serial.println("COREPEER_VALUE_SET core-host-second");
    }
    else if (command == "d" && linkUp)
    {
      const bool requested = static_cast<bool>(server.disconnect(connectionId));
      Serial.printf("COREPEER_DISCONNECT requested=%u\n", requested ? 1 : 0);
      if (disconnectedPending)
      {
        disconnectedPending = false;
        Serial.println("COREPEER_DISCONNECTED");
      }
    }
  }
  delay(10);
}
