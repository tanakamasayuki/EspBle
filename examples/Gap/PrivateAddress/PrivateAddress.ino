// en: Advertise with a random static / resolvable private address
// ja: random static / resolvable private addressでadvertise
#include <EspBle.h>

// en: false = RandomStatic (works standalone), true = RPA (requires bonding).
// ja: false = RandomStatic（単体で動く）、true = RPA（bondingが前提）。
static constexpr bool USE_RESOLVABLE_PRIVATE_ADDRESS = false;

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Private Address";

  if (USE_RESOLVABLE_PRIVATE_ADDRESS)
  {
    // en: An RPA is only useful together with bonding: the peer needs the IRK to
    //     recognise this device across a rotation. Without security enabled, a
    //     peer cannot follow the address change and reconnects fail.
    // ja: RPAはbonding併用でのみ意味を持つ。回転後も同一機器と分かるにはpeerにIRKが
    //     必要なため。securityなしでは相手がアドレス変化を追えず再接続できない。
    config.ownAddressType = EspBleOwnAddressType::ResolvablePrivate;
    config.security.enabled = true;
    config.security.bonding = true;
  }
  else
  {
    // en: A fixed random address. It hides the factory address but, because it
    //     never changes, it can still be used to track this device.
    // ja: 固定のrandom address。工場出荷アドレスは隠せるが、変化しないので
    //     このアドレス自体での追跡は防げない。
    config.ownAddressType = EspBleOwnAddressType::RandomStatic;
  }

  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    // en: peerAddress is what this device sees. If the peer also uses an RPA and
    //     is bonded, the backend resolves it to the identity address.
    // ja: peerAddressはこちらから見えたアドレス。相手もRPAでbonding済みなら、
    //     backendがidentity addressへ解決した結果が入る。
    Serial.printf("Connected id=%u peer=%s bonded=%d\n",
      connection.id, connection.peerAddress.c_str(), connection.bonded ? 1 : 0);
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("Disconnected id=%u\n", connection.id);
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Private Address");
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.printf(
    "Advertising with %s address.\n",
    USE_RESOLVABLE_PRIVATE_ADDRESS ? "a resolvable private (rotating)" : "a random static");
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
