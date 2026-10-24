// en: NUS-compatible RX writes and TX notification echo
// ja: NUS互換RX Write / TX Notification echo
#include <EspBle.h>

static constexpr const char *NUS_SERVICE_UUID =
  "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_RX_UUID =
  "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_TX_UUID =
  "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

EspBle ble;

EspBleGattService nusServiceService;
EspBleGattCharacteristic nusRxCharacteristic;
EspBleGattCharacteristic nusTxCharacteristic;
void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig rxConfig;
  rxConfig.writable = true;
  rxConfig.writableWithoutResponse = true;
  EspBleGattCharacteristicConfig txConfig;
  txConfig.notifiable = true;

  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  if (!(nusServiceService = server.addService(NUS_SERVICE_UUID)).valid() ||
      !(nusRxCharacteristic = server.addCharacteristic(nusServiceService, NUS_RX_UUID, rxConfig)).valid() ||
      !(nusTxCharacteristic = server.addCharacteristic(nusServiceService, NUS_TX_UUID, txConfig)).valid())
  {
    Serial.printf("NUS configuration failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(NUS_RX_UUID)) return;
    Serial.printf("RX: %s\n", write.value.c_str());
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    const bool echoed = ble.gattServer().notify(nusTxCharacteristic, write.value);
    Serial.printf("Echo accepted: %u\n", echoed ? 1 : 0);
  });
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    if (subscription.characteristicUuid.equalsIgnoreCase(NUS_TX_UUID))
    {
      Serial.printf("TX notifications: %u\n", subscription.notifications ? 1 : 0);
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle NUS";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle NUS");
  ble.advertising().addServiceUuid(NUS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
