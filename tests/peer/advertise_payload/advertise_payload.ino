// Raw advertisement payload inspector. Uses the bundled BLE API directly so
// the exact AD structures EspBle advertising produces can be verified against
// the Core Specification Supplement (one AD structure per type).
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

BLEScan *scan = nullptr;
String targetName = "BlePayload";

class Callbacks : public BLEAdvertisedDeviceCallbacks
{
public:
  void onResult(BLEAdvertisedDevice device) override
  {
    if (!device.haveName() || String(device.getName().c_str()) != targetName)
    {
      return;
    }
    uint8_t *payload = device.getPayload();
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
    scan->stop();
  }
};

Callbacks callbacks;

void setup()
{
  Serial.begin(115200);
  delay(500);
  BLEDevice::init("EspBle Payload Scanner");
  scan = BLEDevice::getScan();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's')
    {
      // Passive scan so SCANNER_PAYLOAD reports the advertising PDU itself,
      // not a merged scan response.
      scan->setAdvertisedDeviceCallbacks(&callbacks, true, true);
      scan->setActiveScan(false);
      scan->setInterval(100);
      scan->setWindow(50);
      Serial.printf("SCANNER_STARTED success=%u\n",
        scan->start(0, nullptr, false) ? 1 : 0);
    }
  }
  delay(1);
}
