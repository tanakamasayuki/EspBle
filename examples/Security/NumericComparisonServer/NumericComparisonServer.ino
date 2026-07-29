// en: NumericComparisonServer - the peripheral half of Numeric Comparison pairing.
//     Both sides declare DisplayYesNo and require MITM, so LE Secure Connections
//     shows the same 6-digit value on both devices and each user confirms they
//     match. Nobody types anything: the value is only ever compared.
// ja: NumericComparisonServer - Numeric Comparison PairingのPeripheral側。
//     両側が DisplayYesNo かつMITM要求とすると、LE Secure Connectionsが同じ6桁を
//     両方に表示し、それぞれの利用者が一致を確認する。入力は不要で、値は比較するだけ。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d830-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d831-802e-43e7-9003-706173736b79";

EspBle ble;

EspBleGattService service;
EspBleGattCharacteristic characteristic;
bool awaitingConfirmation = false;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  valueConfig.authenticatedRead = true;

  auto &gattServer = ble.gattServer();
  service = gattServer.addService(SERVICE_UUID);
  characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(characteristic, String("MITM protected value"));

  EspBleConfig config;
  config.deviceName = "EspBle NumCmp";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  // en: display + yes/no. Numeric Comparison is chosen only when both sides
  //     declare this and both require MITM; anything else falls back.
  // ja: 表示＋Yes/No。Numeric Comparisonは両側がこれを申告し、両側がMITMを
  //     要求したときにだけ選ばれる。それ以外の組み合わせでは別の方式になる。
  config.security.ioCapability = EspBleSecurityIoCapability::DisplayYesNo;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onNumericComparison([](const EspBlePasskeyDisplayed &event) {
    awaitingConfirmation = true;
    Serial.printf(
      "Does the peer show %06u? Send 'y' to accept, 'n' to reject.\n",
      static_cast<unsigned>(event.passkey));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    awaitingConfirmation = false;
    Serial.printf(
      "Security %s: encrypted=%u authenticated=%u bonded=%u\n",
      event.success ? "established" : "failed",
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    awaitingConfirmation = false;
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle NumCmp");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();

  Serial.println("Send 'c' while disconnected to clear all bonds.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if ((command == 'y' || command == 'n') && awaitingConfirmation)
    {
      // en: Pairing is blocked waiting for this answer and gives up after 30 s.
      // ja: Pairingはこの答えを待って止まっており、30秒で打ち切られる。
      Serial.printf(
        "Answer %s: %s\n",
        command == 'y' ? "accept" : "reject",
        ble.confirmNumericComparison(command == 'y') ? "sent" : ble.lastErrorName());
    }
    else if (command == 'c')
    {
      Serial.printf(
        "Clear bonds: %s, remaining=%u\n",
        ble.deleteAllBonds() ? "success" : ble.lastErrorName(),
        static_cast<unsigned>(ble.bondCount()));
    }
  }

  ble.update();
  delay(1);
}
