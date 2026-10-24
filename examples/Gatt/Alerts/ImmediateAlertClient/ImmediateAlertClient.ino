// en: Find Me locator: raise/clear Alert Level via Write Without Response
// ja: Find Me locator: Write Without ResponseでAlert Levelを鳴動/解除
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
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    writeAlertLevel(2); // en: High Alert / ja: High Alert
    alertStart = millis();
    alerting = true;
    Serial.println("High Alert sent");
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    alerting = false;
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
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

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
