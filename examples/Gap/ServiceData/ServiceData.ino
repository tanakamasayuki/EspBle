// en: ServiceData - broadcast a Service Data block: a payload tagged with the service
//     UUID it belongs to. This is how a sensor publishes a reading without anyone
//     connecting to it, and unlike Manufacturer Data it needs no company ID.
// ja: ServiceData - Service Dataブロックを放送する。どのserviceの値かをUUIDで示した
//     payloadで、センサーが「接続させずに値を配る」ときの標準的な方法。Manufacturer Data
//     と違い、割り当て済みcompany IDが不要。
#include <EspBle.h>

// en: Environmental Sensing Service (0x181A). Using the SIG-assigned UUID means any
//     scanner knows what the payload is about.
// ja: Environmental Sensing Service（0x181A）。SIG割り当てUUIDを使うと、
//     payloadが何についての値かを任意のスキャナが解釈できる。
static constexpr const char *SERVICE_UUID = "181A";

EspBle ble;
uint32_t lastUpdateMs = 0;
int16_t temperatureCentiCelsius = 2350; // en: 23.50 degC / ja: 23.50 度

// en: Rebuild the Service Data payload and restart advertising so the new value
//     goes out. Legacy advertising has no way to update the payload in place.
// ja: Service Data payloadを作り直してadvertisingを再開し、新しい値を流す。
//     Legacy advertisingにはpayloadをその場で書き換える手段がない。
static void publishTemperature()
{
  // en: Environmental Sensing Temperature is a signed 16-bit value in 0.01 degC,
  //     little-endian -- the same wire format the GATT characteristic uses.
  // ja: Environmental SensingのTemperatureは0.01度単位の符号付き16bit・
  //     little-endian。GATT characteristicと同じwire形式。
  const uint8_t payload[] = {
    static_cast<uint8_t>(temperatureCentiCelsius & 0xFF),
    static_cast<uint8_t>((temperatureCentiCelsius >> 8) & 0xFF),
  };

  auto &advertising = ble.advertising();
  advertising.stop();
  if (!advertising.addServiceData(SERVICE_UUID, payload, sizeof(payload)))
  {
    Serial.printf("Service data failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.printf("Broadcasting %d.%02d degC\n", temperatureCentiCelsius / 100,
    abs(temperatureCentiCelsius) % 100);
}

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Service Data";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  // en: Non-connectable: this is a broadcaster, the value is in the advertisement.
  // ja: non-connectable。値はadvertisementに載っているので接続させない。
  advertising.setConnectable(false);
  advertising.setScanResponseEnabled(false);
  // en: Also list the UUID in the service-UUID list so advertisesService() matches it.
  // ja: service-UUID一覧にも載せ、advertisesService()で絞り込めるようにする。
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.setInterval(500, 600);

  publishTemperature();
}

void loop()
{
  // en: Publish a new reading every 5 seconds so a scanner sees the value change.
  // ja: 5秒ごとに新しい値を流し、スキャナ側で値の変化が見えるようにする。
  if (millis() - lastUpdateMs >= 5000)
  {
    lastUpdateMs = millis();
    temperatureCentiCelsius += 25; // en: fake sensor drift / ja: 疑似的な値の変化
    if (temperatureCentiCelsius > 2600) temperatureCentiCelsius = 2350;
    publishTemperature();
  }

  ble.update();
  delay(1);
}
