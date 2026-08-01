// ESP32-P4 ESP-Hosted custom SDIO pins.
// Prefer selecting the correct Arduino board definition. Use this override only
// for custom hardware or when its board variant is not available.
#include <EspBle.h>

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#include "esp32-hal-hosted.h"
#endif

// M5Stack Tab5 wiring. Replace every value together for other hardware.
static constexpr int8_t HOSTED_CLK = 12;
static constexpr int8_t HOSTED_CMD = 13;
static constexpr int8_t HOSTED_D0 = 11;
static constexpr int8_t HOSTED_D1 = 10;
static constexpr int8_t HOSTED_D2 = 9;
static constexpr int8_t HOSTED_D3 = 8;
static constexpr int8_t HOSTED_RESET = 15;

EspBle ble;
bool bleStarted = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  // This must run before ble.begin() and before any Wi-Fi API initializes the
  // same shared ESP-Hosted transport.
  if (!hostedSetPins(
        HOSTED_CLK, HOSTED_CMD,
        HOSTED_D0, HOSTED_D1, HOSTED_D2, HOSTED_D3,
        HOSTED_RESET))
  {
    Serial.println("ESP-Hosted SDIO pin configuration failed");
    return;
  }

  int8_t clk, cmd, d0, d1, d2, d3, reset;
  hostedGetPins(&clk, &cmd, &d0, &d1, &d2, &d3, &reset);
  Serial.printf(
    "ESP-Hosted pins: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d RESET=%d\n",
    clk, cmd, d0, d1, d2, d3, reset);

  EspBleConfig config;
  config.deviceName = "EspBle Hosted Custom Pins";
  bleStarted = ble.begin(config);
  if (!bleStarted)
  {
    Serial.printf(
      "BLE initialization failed: %s (%s)\n",
      ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.println("BLE initialized through ESP-Hosted");
#else
  Serial.println("This example requires an ESP-Hosted NimBLE board such as ESP32-P4");
#endif
}

void loop()
{
  if (bleStarted) ble.update();
  delay(1);
}
