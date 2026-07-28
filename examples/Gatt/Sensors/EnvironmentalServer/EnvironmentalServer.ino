#include <EspBle.h>

static constexpr const char *ENVIRONMENTAL_SENSING_SERVICE_UUID = "181a";
static constexpr const char *TEMPERATURE_UUID = "2a6e";
static constexpr const char *HUMIDITY_UUID = "2a6f";
static constexpr const char *PRESSURE_UUID = "2a6d";

EspBle ble;
EspBleGattService environmentalSensingServiceService;
EspBleGattCharacteristic temperatureCharacteristic;
EspBleGattCharacteristic humidityCharacteristic;
EspBleGattCharacteristic pressureCharacteristic;
int16_t temperatureHundredths = 2150;
uint8_t temperatureValue[2];
uint8_t humidityValue[2];
uint8_t pressureValue[4];

static void encode16(uint16_t value, uint8_t *output)
{
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
}

static void encode32(uint32_t value, uint8_t *output)
{
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
  output[2] = static_cast<uint8_t>(value >> 16);
  output[3] = static_cast<uint8_t>(value >> 24);
}

static void publishTemperature()
{
  encode16(static_cast<uint16_t>(temperatureHundredths), temperatureValue);
  auto &server = ble.gattServer();
  server.setValue(temperatureCharacteristic, temperatureValue, sizeof(temperatureValue));
  const bool notified = server.notify(temperatureCharacteristic, temperatureValue, sizeof(temperatureValue));
  Serial.printf("Temperature raw: %d (notification accepted: %u)\n",
    temperatureHundredths, notified ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);
  encode16(static_cast<uint16_t>(temperatureHundredths), temperatureValue);
  encode16(4875, humidityValue);       // 48.75 %.
  encode32(1013250, pressureValue);    // 101325.0 Pa.

  EspBleGattCharacteristicConfig temperatureConfig;
  temperatureConfig.readable = true;
  temperatureConfig.notifiable = true;
  EspBleGattCharacteristicConfig readable;
  readable.readable = true;
  auto &server = ble.gattServer();
  if (!(environmentalSensingServiceService = server.addService(ENVIRONMENTAL_SENSING_SERVICE_UUID)).valid() ||
      !(temperatureCharacteristic = server.addCharacteristic(environmentalSensingServiceService, TEMPERATURE_UUID, temperatureConfig)).valid() ||
      !(humidityCharacteristic = server.addCharacteristic(environmentalSensingServiceService, HUMIDITY_UUID, readable)).valid() ||
      !(pressureCharacteristic = server.addCharacteristic(environmentalSensingServiceService, PRESSURE_UUID, readable)).valid() ||
      !server.setValue(temperatureCharacteristic, temperatureValue, sizeof(temperatureValue)) ||
      !server.setValue(humidityCharacteristic, humidityValue, sizeof(humidityValue)) ||
      !server.setValue(pressureCharacteristic, pressureValue, sizeof(pressureValue)))
  {
    Serial.printf("Environmental configuration failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Environmental";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Environmental");
  ble.advertising().addServiceUuid(ENVIRONMENTAL_SENSING_SERVICE_UUID);
  ble.advertising().start();
  Serial.println("Send '+' or '-' to change temperature by 0.25 C.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '+')
    {
      temperatureHundredths += 25;
      publishTemperature();
    }
    else if (command == '-')
    {
      temperatureHundredths -= 25;
      publishTemperature();
    }
  }
  ble.update();
  delay(1);
}
