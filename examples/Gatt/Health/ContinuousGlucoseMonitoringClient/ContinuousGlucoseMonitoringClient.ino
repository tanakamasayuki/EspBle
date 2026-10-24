// en: E2E-CRC verification and SFLOAT glucose/time-offset decoding
// ja: E2E-CRC検証とSFLOAT血糖値/time offsetのdecode
#include <EspBle.h>
#include <EspBleCgmCrc.h>
#include <EspBleMedicalFloat.h>
#include <math.h>

static constexpr const char *CGM_SERVICE_UUID = "181f";
static constexpr const char *CGM_MEASUREMENT_UUID = "2aa7";
static constexpr const char *CGM_FEATURE_UUID = "2aa8";

EspBle ble;

static void copyToBuffer(const String &value, uint8_t *buffer, size_t length)
{
  for (size_t i = 0; i < length; ++i)
    buffer[i] = static_cast<uint8_t>(value[i]);
}

void setup()
{
  Serial.begin(115200);
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    ble.readCharacteristic(connection.id, CGM_SERVICE_UUID, CGM_FEATURE_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(CGM_FEATURE_UUID) ||
        !result.success || result.value.length() != 6)
      return;
    uint8_t buffer[6];
    copyToBuffer(result.value, buffer, 6);
    if (!espBleCgmVerifyCrc(buffer, 6))
    {
      Serial.println("CGM Feature E2E-CRC mismatch");
      return;
    }
    const uint32_t feature = static_cast<uint32_t>(buffer[0]) |
      (static_cast<uint32_t>(buffer[1]) << 8) | (static_cast<uint32_t>(buffer[2]) << 16);
    Serial.printf("CGM Feature: 0x%06x (E2E-CRC verified)\n", static_cast<unsigned>(feature));
    ble.subscribe(result.connectionId, CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID);
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(CGM_MEASUREMENT_UUID) ||
        notification.value.length() != 8)
      return;
    uint8_t buffer[8];
    copyToBuffer(notification.value, buffer, 8);
    if (!espBleCgmVerifyCrc(buffer, 8))
    {
      Serial.println("CGM Measurement E2E-CRC mismatch");
      return;
    }
    const uint8_t sfloat[2] = {buffer[2], buffer[3]};
    const long glucose = lround(espBleReadMedicalSFloatLE(sfloat));
    const unsigned timeOffset = static_cast<unsigned>(buffer[4]) | (static_cast<unsigned>(buffer[5]) << 8);
    Serial.printf("Glucose: %ld mg/dL at +%u min\n", glucose, timeOffset);
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(CGM_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
