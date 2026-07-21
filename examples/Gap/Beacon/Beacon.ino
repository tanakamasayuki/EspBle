// en: Beacon - broadcast a non-connectable, non-scannable beacon carrying
//     manufacturer data. setConnectable(false) makes it a pure broadcaster (no
//     GATT connection possible); setScanResponseEnabled(false) makes it
//     non-scannable; setInterval() controls the advertising interval. Observe it
//     with the Gap/Scan example or any scanner app.
// ja: Beacon - manufacturer dataを載せたnon-connectable・non-scannableなbeaconを
//     broadcastする。setConnectable(false) で純粋なbroadcaster（GATT接続不可）に、
//     setScanResponseEnabled(false) でnon-scannableに、setInterval() で送信間隔を
//     制御する。Gap/Scan exampleや汎用スキャナアプリで受信できる。
#include <EspBle.h>

EspBle ble;

// en: Company ID 0xFFFF (test/unassigned) followed by a small payload. Replace
//     with your assigned company ID and an iBeacon/Eddystone layout as needed.
// ja: Company ID 0xFFFF（テスト用/未割当）＋小さなpayload。必要なら自社の割当
//     company IDや iBeacon/Eddystone のレイアウトに置き換える。
static const uint8_t manufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04};

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Beacon";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setConnectable(false);          // en: pure broadcaster / ja: 純粋なbroadcaster
  advertising.setScanResponseEnabled(false);  // en: non-scannable / ja: non-scannable
  advertising.setManufacturerData(manufacturerData, sizeof(manufacturerData));
  advertising.setInterval(100, 150);          // en: 100..150 ms / ja: 100〜150 ms
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
