// Eddystone broadcaster for the eddystone peer test. Broadcasts a URL frame by
// default; 'u' switches to a UID frame and 't' to a TLM frame.
#include <EspBle.h>
#include <EspBleEddystone.h>

EspBle ble;

static void advertiseFrame(const uint8_t *body, size_t length)
{
  auto &advertising = ble.advertising();
  advertising.stop();
  advertising.clear();
  advertising.setConnectable(false);
  advertising.setScanResponseEnabled(false);
  advertising.addServiceUuid(EspBleEddystoneServiceUuid);
  advertising.setServiceData(EspBleEddystoneServiceUuid, body, length);
  advertising.setInterval(100, 150);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

static void advertiseUrl()
{
  uint8_t body[EspBleEddystoneUrlMaxBodySize];
  size_t length = 0;
  if (espBleEncodeEddystoneUrl("https://www.example.com/", -20, body, sizeof(body), length))
    advertiseFrame(body, length);
}

static void advertiseUid()
{
  EspBleEddystoneUidData uid;
  for (int i = 0; i < 10; ++i) uid.namespaceId[i] = static_cast<uint8_t>(0xA0 + i);
  for (int i = 0; i < 6; ++i) uid.instanceId[i] = static_cast<uint8_t>(0x10 + i);
  uid.txPower = -18;
  uint8_t body[EspBleEddystoneUidBodySize];
  size_t length = 0;
  if (espBleEncodeEddystoneUid(uid, body, sizeof(body), length))
    advertiseFrame(body, length);
}

static void advertiseTlm()
{
  EspBleEddystoneTlmData tlm;
  tlm.batteryMilliVolts = 3300;
  tlm.temperatureCelsius = 25.5f;
  tlm.advertisingCount = 66051;      // 0x00010203
  tlm.uptimeDeciseconds = 168496141; // 0x0A0B0C0D
  uint8_t body[EspBleEddystoneTlmBodySize];
  size_t length = 0;
  if (espBleEncodeEddystoneTlm(tlm, body, sizeof(body), length))
    advertiseFrame(body, length);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Eddystone";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  advertiseUrl();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'r')
    {
      advertiseUrl();
      Serial.println("FRAME_URL");
    }
    else if (command == 'u')
    {
      advertiseUid();
      Serial.println("FRAME_UID");
    }
    else if (command == 't')
    {
      advertiseTlm();
      Serial.println("FRAME_TLM");
    }
  }

  ble.update();
  delay(1);
}
