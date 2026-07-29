// en: DirectedAdvertise - advertise to exactly one peer. A directed advertisement
//     names the target address in the PDU, so only that peer may connect, and it
//     carries no payload at all.
// ja: DirectedAdvertise - 相手を1台に限定してadvertiseする。有向advertisingはPDUに
//     宛先アドレスを載せるため、その相手だけが接続でき、payloadは一切載らない。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "3d9b1c40-6f2e-4a8b-9f31-646972656374";

// en: Address of the central to advertise to. That board can print its own with
//     ble.localAddress(); EspBleAddressType must match its localAddressType().
// ja: advertise先Centralのアドレス。相手のボードでは ble.localAddress() で表示できる。
//     EspBleAddressType は相手の localAddressType() に合わせる。
static constexpr const char *TARGET_CENTRAL = "aa:bb:cc:dd:ee:ff";
static constexpr EspBleAddressType TARGET_TYPE = EspBleAddressType::Public;

EspBle ble;

static void advertiseUndirected()
{
  auto &advertising = ble.advertising();
  // en: Clearing the target restores the normal payload; it was kept while
  //     directed, just not transmitted.
  // ja: targetを解除すると通常のpayloadに戻る。有向中も保持されていて、
  //     送信されていなかっただけ。
  advertising.clearDirectedTarget();
  advertising.start();
  Serial.println("Undirected: anyone may connect.");
}

static void advertiseDirected()
{
  auto &advertising = ble.advertising();
  advertising.stop();
  // en: The third argument selects High Duty Cycle: 3.75 ms interval for at most
  //     1.28 s, for the fastest possible reconnection to a known peer. It stops
  //     by itself when that runs out. false (the default) keeps the configured
  //     interval and advertises until stop().
  // ja: 第3引数でHigh Duty Cycleを選ぶ。3.75 ms間隔で最大1.28秒送出し、既知の相手へ
  //     最速で再接続する。時間切れで自動的に止まる。false（既定）なら設定した間隔で
  //     stop() まで続く。
  if (!advertising.setDirectedTarget(TARGET_CENTRAL, TARGET_TYPE, false))
  {
    Serial.printf("Directed target failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.printf("Directed at %s. No payload is sent.\n", TARGET_CENTRAL);
}

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Directed";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("Connected id=%u from %s\n", connection.id, connection.peerAddress.c_str());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("Disconnected id=%u\n", connection.id);
    // en: Advertising stops when a connection is established; restart it here.
    // ja: 接続が成立するとadvertisingは止まるので、ここで再開する。
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Directed");
  advertising.addServiceUuid(SERVICE_UUID);

  // en: Start undirected so the central can find this device once and learn its
  //     address. A directed advertisement cannot be matched by service UUID
  //     because it has no payload to match against.
  // ja: まず無向で始め、Centralに一度見つけてもらってアドレスを学習させる。
  //     有向advertisingは照合するpayloadを持たないため、Service UUIDでは拾えない。
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.printf("Advertising as %s. Send 'd' to direct it at %s.\n",
    ble.localAddress().c_str(), TARGET_CENTRAL);
}

void loop()
{
  // en: 'd' switches to directed advertising, 'u' back to undirected.
  // ja: 'd' で有向advertisingへ切り替え、'u' で無向へ戻す。
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'd')
    {
      advertiseDirected();
    }
    else if (command == 'u')
    {
      ble.advertising().stop();
      advertiseUndirected();
    }
  }

  ble.update();
  delay(1);
}
