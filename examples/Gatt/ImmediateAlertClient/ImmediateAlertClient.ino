// en: ImmediateAlertClient - the Find Me profile locator role. Connect to an
//     Immediate Alert Service (0x1802) and write Alert Level (0x2A06) with Write
//     Without Response to make the target alert. This example raises High Alert
//     on connect and clears it after five seconds.
// ja: ImmediateAlertClient - Find Meプロファイルのlocator役。Immediate Alert
//     Service（0x1802）へ接続し、Alert Level（0x2A06）へWrite Without Responseで
//     書き込んでターゲットを鳴動させる。この例では接続時にHigh Alertを出し、
//     5秒後に解除する。
#include <EspBle.h>

static constexpr const char *IMMEDIATE_ALERT_SERVICE_UUID = "1802";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";

EspBle ble;
EspBleConnectionId connectionId = 0;
unsigned long alertStart = 0;
bool alerting = false;

static void writeAlertLevel(uint8_t level)
{
  // en: Write Without Response: last argument false.
  // ja: Write Without Response: 最後の引数はfalse。
  ble.writeCharacteristic(
    connectionId, IMMEDIATE_ALERT_SERVICE_UUID, ALERT_LEVEL_UUID, &level, sizeof(level), false);
}

void setup()
{
  Serial.begin(115200);
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    writeAlertLevel(2); // en: High Alert / ja: High Alert
    alertStart = millis();
    alerting = true;
    Serial.println("High Alert sent");
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    alerting = false;
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(IMMEDIATE_ALERT_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  // en: Clear the alert five seconds after raising it.
  // ja: 鳴動開始から5秒後に解除する。
  if (alerting && connectionId != 0 && millis() - alertStart >= 5000)
  {
    alerting = false;
    writeAlertLevel(0); // en: No Alert / ja: No Alert
    Serial.println("No Alert sent");
  }

  ble.update();
  delay(1);
}
