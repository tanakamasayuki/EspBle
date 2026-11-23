#include <Arduino.h>
#include <BLE.h>

#include "../../sketch_support/EspBleTestLifecycle.h"

#if !defined(CONFIG_NIMBLE_ENABLED)
#error "EspBle peer tests require the Arduino-ESP32 NimBLE backend"
#endif

static const BLEUUID serviceUuid("8d47a620-8d3a-4d65-a76f-6f7370626c65");
static const BLEUUID characteristicUuid("8d47a621-8d3a-4d65-a76f-6f7370626c65");
static BLEAdvertisedDevice peer;
static volatile bool connectPending = false;
static bool started = false;
static bool complete = false;
static BLEClient client;
static bool stackStopped = false;

// Runs on the BLE stack task: keep it short and leave the connect to loop().
static void onScanResult(const BLEAdvertisedDevice &device)
{
  if (connectPending || !device.haveServiceUUID() || !device.isAdvertisingService(serviceUuid))
  {
    return;
  }

  peer = device;
  connectPending = true;
  Serial.println("SCAN_FOUND");
}

static bool connectAndExerciseGatt()
{
  client = BLE.createClient();
  if (!client || !client.connect(peer))
  {
    return false;
  }
  Serial.println("CONNECTED");

  BLERemoteService service = client.getService(serviceUuid);
  if (!service)
  {
    return false;
  }

  BLERemoteCharacteristic characteristic = service.getCharacteristic(characteristicUuid);
  if (!characteristic)
  {
    return false;
  }

  String value = characteristic.readValue();
  Serial.printf("READ %s\n", value.c_str());
  char payload[] = "central-write";
  const bool wrote = static_cast<bool>(characteristic.writeValue(
    reinterpret_cast<const uint8_t *>(payload), sizeof(payload) - 1, true));
  Serial.printf("WRITE %u\n", wrote ? 1 : 0);
  return wrote;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLE.begin(""))
  {
    Serial.println("CENTRAL_INIT_FAILED");
    return;
  }

  BLEScan scan = BLE.getScan();
  scan.onResult(onScanResult);
  scan.setActiveScan(true);
  EspBleTestLifecycle::announceReady([] { Serial.println("CENTRAL_READY"); });

  // RECOVER leaves the sketch idle rather than scanning again: the boot scan
  // reconnects to the peer the moment it sees it, the opposite of the state
  // the command asks for. end() takes the radio down synchronously, so STOP
  // is complete at once.
  EspBleTestLifecycle::Hooks hooks;
  hooks.stop = []() {
    stackStopped = true;
    BLE.end(false);
  };
  EspBleTestLifecycle::begin(hooks);
  // Scanning starts on 'g', not here: the connection and GATT lines it leads to
  // are gone if they are printed before the serial reader attaches.
}

void loop()
{
  if (Serial.available() > 0 && EspBleTestLifecycle::filter(Serial.read()) == 'g')
  {
    started = true;
  }
  EspBleTestLifecycle::update();
  // STOP has taken the stack down; nothing below may call into it any more.
  if (stackStopped)
  {
    delay(10);
    return;
  }
  if (connectPending && !complete)
  {
    // Stopped here rather than in onScanResult: stopping a scan from inside its
    // own callback stalls the BLE task.
    BLE.getScan().stop();
    connectPending = false;
    complete = connectAndExerciseGatt();
    if (!complete)
    {
      Serial.println("CONNECT_OR_GATT_FAILED");
    }
  }
  else if (started && !complete && !connectPending)
  {
    BLEScan scan = BLE.getScan();
    if (!scan.isScanning())
    {
      scan.start(5000);
    }
  }
  delay(10);
}
