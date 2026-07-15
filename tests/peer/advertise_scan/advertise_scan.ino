#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "8d47a630-8d3a-4d65-a76f-6f7370626c65";

EspBle ble;
TaskHandle_t loopTask = nullptr;
bool scanStarted = false;

static String bytesToHex(const String &value)
{
  static constexpr char hex[] = "0123456789abcdef";
  String result;
  result.reserve(value.length() * 2);
  for (size_t index = 0; index < value.length(); ++index)
  {
    const uint8_t byte = static_cast<uint8_t>(value[index]);
    result += hex[byte >> 4];
    result += hex[byte & 0x0f];
  }
  return result;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleConfig config;
  config.deviceName = "EspBle Scanner";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (!scanResult.advertisesService(TEST_SERVICE_UUID))
    {
      return;
    }

    const bool inLoopContext = xTaskGetCurrentTaskHandle() == loopTask;
    Serial.printf(
      "SCAN_FOUND name=%s service=1 manufacturer=%s context=%s\n",
      scanResult.name.c_str(),
      bytesToHex(scanResult.manufacturerData).c_str(),
      inLoopContext ? "loop" : "stack");
    ble.scanner().stop();
    Serial.printf("SCAN_STOPPED dropped=%u\n", static_cast<unsigned>(ble.scanner().droppedResultCount()));
  });
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 's' && !scanStarted)
  {
    EspBleScanConfig scanConfig;
    scanConfig.active = true;
    scanConfig.durationSeconds = 0;
    scanStarted = ble.scanner().start(scanConfig);
    Serial.println(scanStarted ? "SCAN_STARTED" : "SCAN_START_FAILED");
  }

  ble.update();
  delay(1);
}
