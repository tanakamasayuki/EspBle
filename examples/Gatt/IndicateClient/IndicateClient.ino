#include <EspBle.h>

// Indication counterpart of SubscribeClient: subscribes with
// notifications=false, so the stack acknowledges every received value and
// the server's onSent() reports the confirmed delivery.
// Pair with Gatt/IndicateServer.

static constexpr const char *SERVICE_UUID = "43a2c560-71b4-49bd-9003-696e64696361";
static constexpr const char *CHARACTERISTIC_UUID = "43a2c561-71b4-49bd-9003-696e64696361";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Indicate Client";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    // notifications=false subscribes to indications (writes 0x0002 to the CCCD).
    if (!ble.subscribe(connection.id, SERVICE_UUID, CHARACTERISTIC_UUID, false))
    {
      Serial.printf("Subscribe request failed: %s\n", ble.lastErrorDetail().c_str());
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Subscribe failed: %s\n", result.detail.c_str());
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf(
      "%s: %s\n",
      notification.indication ? "Indication" : "Notification",
      notification.value.c_str());
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  ble.scanner().start(scanConfig);
}

void loop()
{
  ble.update();
  delay(1);
}
