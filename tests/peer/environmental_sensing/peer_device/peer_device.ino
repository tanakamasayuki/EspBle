#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *ENVIRONMENTAL_SENSING_SERVICE_UUID = "181a";
static constexpr const char *TEMPERATURE_UUID = "2a6e";
static constexpr const char *HUMIDITY_UUID = "2a6f";
static constexpr const char *PRESSURE_UUID = "2a6d";

EspBle ble;
EspBleGattService environmentalSensingServiceService;
EspBleGattCharacteristic temperatureCharacteristic;
EspBleGattCharacteristic humidityCharacteristic;
EspBleGattCharacteristic pressureCharacteristic;
TaskHandle_t loopTask = nullptr;
uint8_t temperatureValue[2];
uint8_t humidityValue[2];
uint8_t pressureValue[4];

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

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

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();
  encode16(static_cast<uint16_t>(static_cast<int16_t>(-525)), temperatureValue);
  encode16(4875, humidityValue);
  encode32(1013250, pressureValue);

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
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("ENV_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("ENV_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0,
      static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle ENV Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle ENV Peer");
  ble.advertising().addServiceUuid(ENVIRONMENTAL_SENSING_SERVICE_UUID);
  ble.advertising().start();
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
    else if (command == 't')
    {
      encode16(2345, temperatureValue);
      const bool stored = ble.gattServer().setValue(temperatureCharacteristic, temperatureValue, sizeof(temperatureValue));
      const bool notified = ble.gattServer().notify(temperatureCharacteristic, temperatureValue, sizeof(temperatureValue));
      Serial.printf("ENV_UPDATED stored=%u notified=%u raw=2345\n",
        stored ? 1 : 0, notified ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
