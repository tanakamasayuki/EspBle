// en: PrivateAddress - advertise with a private address instead of the factory public
//     address, so passers-by cannot track this device by its address.
//       RandomStatic      = a fixed random address (hides the public one, never rotates)
//       ResolvablePrivate = an RPA the controller rotates; a bonded peer resolves it
//                           with the IRK exchanged at bonding, everyone else sees a
//                           changing address
//     Set USE_RESOLVABLE_PRIVATE_ADDRESS to switch. Observe with Gap/Scan or
//     Info/ScanDump: the address type shows as Random either way.
// ja: PrivateAddress - 工場出荷のpublic addressの代わりにprivate addressでadvertiseし、
//     周囲からアドレスで追跡されないようにする。
//       RandomStatic      = 固定random address（public addressを隠すが回転はしない）
//       ResolvablePrivate = controllerが回転させるRPA。bonded peerはbonding時に交換した
//                           IRKで解決でき、それ以外からは変化するアドレスに見える
//     USE_RESOLVABLE_PRIVATE_ADDRESS で切り替える。Gap/ScanやInfo/ScanDumpで受信すると
//     どちらもaddress typeはRandomになる。
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

  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    // en: peerAddress is what this device sees. If the peer also uses an RPA and
    //     is bonded, the backend resolves it to the identity address.
    // ja: peerAddressはこちらから見えたアドレス。相手もRPAでbonding済みなら、
    //     backendがidentity addressへ解決した結果が入る。
    Serial.printf("Connected id=%u peer=%s bonded=%d\n",
      connection.id, connection.peerAddress.c_str(), connection.bonded ? 1 : 0);
  });
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
  ble.update();
  delay(1);
}
