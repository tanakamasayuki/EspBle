// en: Non-connectable, non-scannable beacon with manufacturer data and interval control
// ja: manufacturer data＋間隔制御のnon-connectable・non-scannable beacon
#include <EspBle.h>

EspBle ble;

// en: Company ID 0xFFFF (test/unassigned) followed by a small payload. Replace
//     it with your assigned company ID and your own layout. For iBeacon, use
//     EspBleIBeaconData and the Gap/IBeacon example instead of hand-packing it.
// ja: Company ID 0xFFFF（テスト用/未割当）＋小さなpayload。自社の割当company IDと
//     独自レイアウトに置き換える。iBeaconなら手で詰めずEspBleIBeaconDataと
//     Gap/IBeacon exampleを使う。
static const uint8_t manufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04};

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Beacon";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
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
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
