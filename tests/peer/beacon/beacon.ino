// beacon DUT: EspBle scanner that captures a non-connectable beacon and reports
// its connectability, scannability, and manufacturer-data payload. Confirms that
// setConnectable(false) + setScanResponseEnabled(false) produces a non-connectable
// non-scannable advertisement carrying the expected payload.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *MARKER_SERVICE_UUID = "1815";

EspBle ble;
TaskHandle_t loopTask = nullptr;
bool reported = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();
  if (!ble.begin())
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (reported || !result.advertisesService(MARKER_SERVICE_UUID)) return;
    reported = true;
    String hex;
    for (size_t index = 0; index < result.manufacturerData.length(); ++index)
    {
      char byteHex[3];
      snprintf(byteHex, sizeof(byteHex), "%02x", static_cast<uint8_t>(result.manufacturerData[index]));
      hex += byteHex;
    }
    Serial.printf("BEACON connectable=%u scannable=%u mfglen=%u mfg=%s context=%s\n",
      result.connectable ? 1 : 0,
      result.scannable ? 1 : 0,
      static_cast<unsigned>(result.manufacturerData.length()),
      hex.c_str(),
      contextName());
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's')
    {
      reported = false;
      EspBleScanConfig scan;
      scan.active = true; // active scan; a non-scannable beacon still won't answer scan requests
      Serial.println(ble.scanner().start(scan) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'x')
    {
      Serial.println(ble.scanner().stop() ? "SCAN_STOPPED" : "SCAN_STOP_FAILED");
    }
  }
  ble.update();
  delay(1);
}
