// en: NumericComparisonClient - the central half of Numeric Comparison pairing.
//     It declares DisplayYesNo and requires MITM, exactly like the server, so the
//     same 6-digit value appears on both. After both users confirm, it reads a
//     characteristic that only an authenticated link may read.
// ja: NumericComparisonClient - Numeric Comparison PairingのCentral側。
//     Server側と同じく DisplayYesNo かつMITM要求とするため、同じ6桁が両方に出る。
//     両者が確認したあと、認証済みlinkでしか読めないCharacteristicをReadする。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d830-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d831-802e-43e7-9003-706173736b79";

EspBle ble;
bool connectionRequested = false;
bool awaitingConfirmation = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle NumCmp Client";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
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
    if (event.success)
    {
      ble.discoverCharacteristic(event.connection.id, SERVICE_UUID, CHARACTERISTIC_UUID);
    }
  });
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    if (result.success)
    {
      ble.readCharacteristic(result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID);
    }
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success)
    {
      Serial.printf("Protected value: %s\n", result.value.c_str());
    }
    else
    {
      Serial.printf("Read failed: %s\n", result.detail.c_str());
    }
  });
  ble.onDisconnected([](const EspBleConnection &) {
    connectionRequested = false;
    awaitingConfirmation = false;
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  ble.scanner().start(scanConfig);

  Serial.println("Send 'c' while disconnected to clear all bonds.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if ((command == 'y' || command == 'n') && awaitingConfirmation)
    {
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
