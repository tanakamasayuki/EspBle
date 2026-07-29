// en: AcceptList - one Filter Accept List, two uses. It restricts who may connect
//     to this device (BLE has no "approve this connection request" callback: the
//     controller decides before the application hears about it), and it also
//     filters which advertisers a scan reports.
// ja: AcceptList - 1つのFilter Accept Listを2通りに使う。この機器へ接続できる相手を
//     制限し（BLEには「接続要求を承認する」callbackが無く、アプリに届く前に
//     controllerが判断する）、同じリストでscanに報告される相手も絞り込む。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-6163636570";

// en: Replace with the address of the peer you want to allow. That board can
//     print its own address with ble.localAddress().
// ja: 許可したい相手のアドレスに置き換える。相手のボードでは
//     ble.localAddress() で自分のアドレスを表示できる。
static constexpr const char *ALLOWED_PEER = "aa:bb:cc:dd:ee:ff";

EspBle ble;

// en: Whether the running scan is filtered by the accept list.
// ja: 実行中のscanがaccept listで絞り込まれているか。
static bool scanFiltered = false;

static void startScan(bool filtered)
{
  ble.scanner().stop();

  EspBleScanConfig scan;
  // en: With acceptListOnly the controller drops advertisements from anyone not
  //     on the list, so they never reach onResult at all. Cheaper and more
  //     reliable than comparing addresses in the callback.
  // ja: acceptListOnlyを立てると、リストに無い相手のadvertisementはcontrollerが捨て、
  //     onResultへ届かない。callbackでアドレスを比較するより安く、確実。
  scan.acceptListOnly = filtered;
  scan.durationSeconds = 5;

  if (!ble.scanner().start(scan))
  {
    Serial.printf("Scan failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  scanFiltered = filtered;
  Serial.printf("Scanning for 5 s (%s)\n", filtered ? "accept list only" : "everyone");
}

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

  // en: The accept list lives in the controller and is shared by advertising and
  //     scanning. Entries are matched by address, so a peer that rotates an RPA
  //     can only be listed usefully once bonded (then its identity address is
  //     what matters).
  // ja: accept listはcontroller側にあり、advertisingとscanで共通。照合はアドレス単位
  //     なので、RPAを回転させる相手はbonding後（identity addressが効くようになって
  //     から）でないと登録できない。
  if (!ble.addToAcceptList(ALLOWED_PEER, EspBleAddressType::Public))
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

  ble.scanner().onResult([](const EspBleScanResult &result) {
    Serial.printf(
      "Advertiser %s rssi=%d (%s scan)\n",
      result.address.c_str(),
      result.rssi,
      scanFiltered ? "filtered" : "open");
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
  Serial.printf("Advertising. Only %s may connect.\n", ALLOWED_PEER);
  Serial.println("Commands: 'o' open policy, 'r' restrict, 'f' filtered scan, 'a' scan everyone");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    // en: 'o' opens the policy so any central may connect again, 'r' restricts it.
    // ja: 'o' でpolicyを開放して誰でも接続可能にし、'r' で再び制限する。
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
    // en: The same list on the scan side: 'f' reports only listed advertisers.
    // ja: 同じリストをscan側で使う。'f' はリストに載る相手だけを報告する。
    else if (command == 'f' || command == 'a')
    {
      startScan(command == 'f');
    }
  }

  ble.update();
  delay(1);
}
