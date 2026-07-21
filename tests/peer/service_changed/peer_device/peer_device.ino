// service_changed peer_device: EspBle GATT server (peripheral) that advertises a
// marker service and, on command, sends a GATT Service Changed indication via
// notifyServicesChanged(). The Generic Attribute service (0x1801) and its
// Service Changed characteristic (0x2A05) are provided by the backend.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *MARKER_SERVICE_UUID = "1815"; // advertised marker

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig markerConfig;
  markerConfig.readable = true;
  auto &server = ble.gattServer();
  const uint8_t marker = 0;
  if (!server.addService(MARKER_SERVICE_UUID) ||
      !server.addCharacteristic(MARKER_SERVICE_UUID, "2ae2", markerConfig) ||
      !server.setValue(MARKER_SERVICE_UUID, "2ae2", &marker, sizeof(marker)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle SvcChg Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle SvcChg Peer");
  ble.advertising().addServiceUuid(MARKER_SERVICE_UUID);
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
    else if (command == 'c')
    {
      // Indicate that the whole attribute database changed.
      const bool ok = ble.notifyServicesChanged(0x0001, 0xFFFF);
      Serial.printf("SERVICE_CHANGED sent=%u\n", ok ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
