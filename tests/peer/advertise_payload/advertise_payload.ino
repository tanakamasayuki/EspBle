// Raw advertisement payload inspector. Uses the bundled BLE API directly so
// the exact AD structures EspBle advertising produces can be verified against
// the Core Specification Supplement (one AD structure per type).
#include <BLE.h>

#include "../../sketch_support/EspBleTestLifecycle.h"

BLEScan scan;
String targetName = "BlePayload";
bool stackStopped = false;
// Set by the scan callback once the payload is printed; loop() stops the scan,
// since stopping it from inside its own callback stalls the BLE task.
volatile bool payloadReported = false;

// Runs on the BLE stack task.
static void onScanResult(const BLEAdvertisedDevice &device)
{
  if (payloadReported || !device.haveName() || device.getName() != targetName)
  {
    return;
  }
  const uint8_t *payload = device.getPayload();
  const size_t length = device.getPayloadLength();
  String hex;
  for (size_t index = 0; index < length; ++index)
  {
    char buffer[3];
    snprintf(buffer, sizeof(buffer), "%02x", payload[index]);
    hex += buffer;
  }
  Serial.printf("SCANNER_PAYLOAD len=%u hex=%s\n",
    static_cast<unsigned>(length), hex.c_str());
  payloadReported = true;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  BLE.begin("EspBle Payload Scanner");
  scan = BLE.getScan();

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
  if (payloadReported && !stackStopped && scan.isScanning())
  {
    scan.stop();
  }
  if (Serial.available() > 0)
  {
    const char command = EspBleTestLifecycle::filter(Serial.read());
    // STOP has taken the stack down; the scan object went with it.
    if (command == 's' && !stackStopped)
    {
      // Passive scan so SCANNER_PAYLOAD reports the advertising PDU itself,
      // not a merged scan response.
      // Duplicates are reported, as the old wantDuplicates=true did.
      payloadReported = false;
      scan.onResult(onScanResult);
      scan.setFilterDuplicates(false);
      scan.setActiveScan(false);
      scan.setInterval(100);
      scan.setWindow(50);
      Serial.printf("SCANNER_STARTED success=%u\n",
        scan.start(0) ? 1 : 0);
    }
  }
  delay(1);
}
