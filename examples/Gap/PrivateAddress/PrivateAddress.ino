// en: PrivateAddress - advertise with a private address instead of the factory
//     public address. ownAddressType = RandomStatic uses a fixed random address;
//     ResolvablePrivate uses a Resolvable Private Address (RPA) that the
//     controller rotates (only useful with security/bonding, since a bonded peer
//     resolves it via the IRK). Observe with the Gap/Scan example: the address
//     type shows as Random.
// ja: PrivateAddress - 工場出荷のpublic addressの代わりにprivate addressで
//     advertiseする。ownAddressType = RandomStatic は固定random address、
//     ResolvablePrivate はcontrollerが回転させるRPA（bonded peerがIRKで解決する
//     ため security/bonding併用時のみ有用）。Gap/Scan exampleで受信するとaddress
//     typeがRandomになる。
#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Private Address";
  // en: RandomStatic hides the public address with a fixed random one.
  //     Switch to ResolvablePrivate (with security enabled) for a rotating RPA.
  // ja: RandomStaticはpublic addressを固定randomで隠す。security有効時に
  //     ResolvablePrivateへ変えると回転RPAになる。
  config.ownAddressType = EspBleOwnAddressType::RandomStatic;
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Private Address");
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  ble.update();
  delay(1);
}
