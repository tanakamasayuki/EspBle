// en: IBeacon - broadcast an Apple iBeacon. The iBeacon layout (proximity UUID
//     + major + minor + measured power) is built by the backend-independent
//     EspBleIBeacon.h codec and sent as manufacturer data on a non-connectable,
//     non-scannable advertisement. Observe it with the Gap/Scan example or any
//     beacon app.
// ja: IBeacon - Apple iBeaconをbroadcastする。iBeaconレイアウト（proximity UUID
//     ＋major＋minor＋measured power）はbackend非依存のEspBleIBeacon.h codecで
//     組み立て、non-connectable・non-scannableなadvertisementのmanufacturer data
//     として送信する。Gap/Scan exampleやbeaconアプリで受信できる。
#include <EspBle.h>
#include <EspBleIBeacon.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle iBeacon";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleIBeaconData beacon;
  // en: Replace with your own proximity UUID / major / minor.
  // ja: 自分のproximity UUID / major / minorに置き換える。
  const uint8_t uuid[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
  memcpy(beacon.uuid, uuid, 16);
  beacon.major = 100;
  beacon.minor = 1;
  beacon.measuredPower = -59; // en: calibrated RSSI at 1 m / ja: 1 m時の校正RSSI

  uint8_t payload[EspBleIBeaconManufacturerDataSize];
  espBleEncodeIBeacon(beacon, payload);

  auto &advertising = ble.advertising();
  advertising.setConnectable(false);          // en: pure broadcaster / ja: 純粋なbroadcaster
  advertising.setScanResponseEnabled(false);  // en: non-scannable / ja: non-scannable
  advertising.setManufacturerData(payload, sizeof(payload));
  advertising.setInterval(100, 150);
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  ble.update();
  delay(1);
}
