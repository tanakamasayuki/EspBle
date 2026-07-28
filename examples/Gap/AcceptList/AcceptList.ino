// en: AcceptList - restrict who may connect. BLE has no "approve this connection
//     request" callback: the decision is made by the controller against the Filter
//     Accept List before the application ever hears about it.
// ja: AcceptList - 接続できる相手を制限する。BLEには「接続要求を承認する」callbackは
//     存在せず、Filter Accept Listに基づいてcontrollerが判断する。アプリには届かない。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-6163636570";

// en: Replace with the address of the central you want to allow. That board can
//     print its own address with ble.localAddress().
// ja: 接続を許可したいCentralのアドレスに置き換える。相手のボードでは
//     ble.localAddress() で自分のアドレスを表示できる。
static constexpr const char *ALLOWED_CENTRAL = "aa:bb:cc:dd:ee:ff";

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Accept List";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  // en: The accept list lives in the controller. Entries are matched by address,
  //     so a peer that rotates an RPA can only be listed usefully once bonded
  //     (then its identity address is what matters).
  // ja: accept listはcontroller側にある。照合はアドレス単位なので、RPAを回転させる
  //     相手はbonding後（identity addressが効くようになってから）でないと登録できない。
  if (!ble.addToAcceptList(ALLOWED_CENTRAL, EspBleAddressType::Public))
  {
    Serial.printf("Accept list failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    // en: Only a peer on the accept list ever reaches this point.
    // ja: accept listに載っている相手だけがここへ到達する。
    Serial.printf("Connected id=%u from %s\n", connection.id, connection.peerAddress.c_str());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("Disconnected id=%u\n", connection.id);
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Accept List");
  advertising.addServiceUuid(SERVICE_UUID);
  // en: ConnectionFromAcceptList still lets anyone scan and see this device; it
  //     only rejects connection requests. Use Both to also hide from scan
  //     requests, or ScanRequestFromAcceptList to filter only those.
  // ja: ConnectionFromAcceptListはscan自体は誰にでも許し、接続要求だけを拒否する。
  //     scan requestも制限するならBoth、scan requestだけならScanRequestFromAcceptList。
  advertising.setFilterPolicy(EspBleAdvertisingFilterPolicy::ConnectionFromAcceptList);

  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.printf("Advertising. Only %s may connect.\n", ALLOWED_CENTRAL);
}

void loop()
{
  // en: 'o' opens the policy so any central may connect again, 'r' restricts it.
  // ja: 'o' でpolicyを開放して誰でも接続可能にし、'r' で再び制限する。
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'o' || command == 'r')
    {
      auto &advertising = ble.advertising();
      advertising.stop();
      advertising.setFilterPolicy(
        command == 'o' ? EspBleAdvertisingFilterPolicy::Any
                       : EspBleAdvertisingFilterPolicy::ConnectionFromAcceptList);
      advertising.start();
      Serial.printf(
        "Policy: %s (accept list has %u entries)\n",
        command == 'o' ? "open" : "restricted",
        static_cast<unsigned>(ble.acceptListCount()));
    }
  }

  ble.update();
  delay(1);
}
