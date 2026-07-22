// en: Eddystone - broadcast an Eddystone-URL beacon. The URL frame (scheme +
//     domain-suffix compression) is built by the backend-independent
//     EspBleEddystone.h codec and sent as Service Data under UUID 0xFEAA on a
//     non-connectable, non-scannable advertisement. Observe it with the Gap/Scan
//     example or any Eddystone-aware beacon app.
// ja: Eddystone - Eddystone-URL beaconをbroadcastする。URL frame（scheme＋ドメイン
//     サフィックス圧縮）はbackend非依存のEspBleEddystone.h codecで組み立て、UUID
//     0xFEAAのService Dataとしてnon-connectable・non-scannableなadvertisementで
//     送信する。Gap/Scan exampleやEddystone対応beaconアプリで受信できる。
#include <EspBle.h>
#include <EspBleEddystone.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Eddystone";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  // en: Build the URL frame. txPower is the calibrated RSSI at 0 m (dBm).
  // ja: URL frameを組む。txPowerは0 m時の校正RSSI（dBm）。
  uint8_t body[EspBleEddystoneUrlMaxBodySize];
  size_t length = 0;
  if (!espBleEncodeEddystoneUrl("https://www.example.com/", -20, body, sizeof(body), length))
  {
    Serial.println("URL does not fit / unknown scheme");
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setConnectable(false);          // en: pure broadcaster / ja: 純粋なbroadcaster
  advertising.setScanResponseEnabled(false);  // en: non-scannable / ja: non-scannable
  advertising.addServiceUuid(EspBleEddystoneServiceUuid); // en: also list 0xFEAA / ja: 0xFEAAを一覧にも載せる
  advertising.setServiceData(EspBleEddystoneServiceUuid, body, length);
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
