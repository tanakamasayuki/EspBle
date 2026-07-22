#include <EspBle.h>

static constexpr const char *ENVIRONMENTAL_SENSING_SERVICE_UUID = "181a";
static constexpr const char *TEMPERATURE_UUID = "2a6e";
static constexpr const char *HUMIDITY_UUID = "2a6f";
static constexpr const char *PRESSURE_UUID = "2a6d";

EspBle ble;
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
  server.setValue(ENVIRONMENTAL_SENSING_SERVICE_UUID, TEMPERATURE_UUID,
    temperatureValue, sizeof(temperatureValue));
  const bool notified = server.notify(
    ENVIRONMENTAL_SENSING_SERVICE_UUID, TEMPERATURE_UUID,
    temperatureValue, sizeof(temperatureValue));
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
  if (!server.addService(ENVIRONMENTAL_SENSING_SERVICE_UUID) ||
      !server.addCharacteristic(
        ENVIRONMENTAL_SENSING_SERVICE_UUID, TEMPERATURE_UUID, temperatureConfig) ||
      !server.addCharacteristic(
        ENVIRONMENTAL_SENSING_SERVICE_UUID, HUMIDITY_UUID, readable) ||
      !server.addCharacteristic(
        ENVIRONMENTAL_SENSING_SERVICE_UUID, PRESSURE_UUID, readable) ||
      !server.setValue(ENVIRONMENTAL_SENSING_SERVICE_UUID, TEMPERATURE_UUID,
        temperatureValue, sizeof(temperatureValue)) ||
      !server.setValue(ENVIRONMENTAL_SENSING_SERVICE_UUID, HUMIDITY_UUID,
        humidityValue, sizeof(humidityValue)) ||
      !server.setValue(ENVIRONMENTAL_SENSING_SERVICE_UUID, PRESSURE_UUID,
        pressureValue, sizeof(pressureValue)))
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
