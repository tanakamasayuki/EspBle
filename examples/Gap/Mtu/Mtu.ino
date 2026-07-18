// en: Mtu - request a larger ATT MTU before connecting and observe the negotiated value.
//     The preferred MTU is set in the config passed to begin(); the backend exchanges
//     it during connection establishment.
// ja: Mtu - 接続前に大きめのATT MTUを希望値として設定し、交換結果を観察する。
//     希望MTUは begin() へ渡すconfigで指定し、backendが接続確立時に交換する。
#include <EspBle.h>

// en: Matches the service UUID of the Gatt/NotifyServer example.
// ja: Gatt/NotifyServer example のService UUIDに合わせてある。
static constexpr const char *TARGET_SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle MTU Central";
  // en: preferred ATT MTU (23-517; out of range is rejected by begin())
  // ja: 希望ATT MTU（23〜517。範囲外は begin() が拒否）
  config.preferredMtu = 185;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    // en: connection.mtu is the snapshot taken at connection; maximumNotificationPayload()
    //     is mtu-3 (excluding the ATT notification header).
    // ja: connection.mtu は接続完了時のsnapshot。maximumNotificationPayload() は
    //     mtu-3（ATT notification header分を除いた値）。
    Serial.printf(
      "Connected with MTU %u (notification payload up to %u bytes)\n",
      connection.mtu,
      static_cast<unsigned>(connection.maximumNotificationPayload()));
  });
  ble.onMtuChanged([](const EspBleMtuChanged &event) {
    Serial.printf("MTU changed from %u to %u\n", event.previousMtu, event.connection.mtu);
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(TARGET_SERVICE_UUID))
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
