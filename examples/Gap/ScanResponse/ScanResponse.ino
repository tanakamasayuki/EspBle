// en: ScanResponse - split the advertised data across two payloads. Legacy advertising
//     gives 31 bytes; an active scanner asks for a second 31-byte scan response, so
//     putting the bulky fields there doubles the budget.
// ja: ScanResponse - 広告データを2つのpayloadへ分ける。Legacy advertisingは31byteだが、
//     active scannerは2つ目の31byteであるscan responseを要求するので、かさばる項目を
//     そちらへ置くと使える容量が倍になる。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-7363616e7270";

// en: Appearance 0x0341 = Generic Thermometer. A phone shows a matching icon.
// ja: Appearance 0x0341 = Generic Thermometer。スマホ側でアイコン表示に使われる。
static constexpr uint16_t APPEARANCE_THERMOMETER = 0x0341;

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Scan Response";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();

  // en: Every AD structure costs 2 bytes of overhead (length + type) on top of its
  //     value, and each payload has 31 bytes total. Budget both sides deliberately;
  //     start() fails with InvalidArgument and names the field that did not fit.
  // ja: AD構造は値のほかに2byte（length + type）のオーバーヘッドを持ち、各payloadの
  //     上限は31byte。両面とも意識して配分する。溢れると start() が InvalidArgument で
  //     失敗し、入らなかったフィールド名がエラーに出る。

  // en: Advertising payload -- what every scanner sees, including passive ones.
  //     Keep it to what a scanner needs to decide "is this the device I want?".
  //       flags       3 (added automatically)
  //       128-bit UUID 18
  //       appearance   4
  //       tx power     3
  //                  = 28 of 31
  // ja: Advertising payload。passive scannerを含む全員に見える面。
  //     「これは目的の機器か？」の判断に要るものだけを置く。
  //       flags        3（自動付与）
  //       128bit UUID 18
  //       appearance   4
  //       tx power     3
  //                  = 31byte中28byte
  advertising.data().addServiceUuid(SERVICE_UUID);
  advertising.data().setAppearance(APPEARANCE_THERMOMETER);
  // en: The controller fills in the real transmit power; a scanner can combine it
  //     with the RSSI to estimate distance. It belongs in the advertising payload
  //     so a passive scanner can use it too.
  // ja: 実際の送信電力はcontrollerが埋める。受信側はRSSIと組み合わせて距離を推定できる。
  //     passive scannerにも使わせたいのでadvertising payload側へ置く。
  advertising.data().setTxPowerIncluded(true);

  // en: Scan response payload -- a second 31 bytes, delivered only to a scanner
  //     that actively requests it. The bulky descriptive fields belong here.
  //       name "EspBle Scan Response" 22
  //       manufacturer data            7
  //                                  = 29 of 31
  // ja: Scan response payload。active scanで要求してきた相手にだけ届く2面目の31byte。
  //     かさばる説明的な項目はこちらへ置く。
  //       name "EspBle Scan Response" 22
  //       manufacturer data            7
  //                                  = 31byte中29byte
  const uint8_t manufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03};
  advertising.scanResponse().setName("EspBle Scan Response");
  advertising.scanResponse().setManufacturerData(manufacturerData, sizeof(manufacturerData));

  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.println("Advertising. Passive scanners see only the service UUID.");
}

void loop()
{
  ble.update();
  delay(1);
}
