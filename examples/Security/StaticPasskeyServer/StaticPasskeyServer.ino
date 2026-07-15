#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d810-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d811-802e-43e7-9003-706173736b79";

// Example-only fixed value. Production devices should provision a unique passkey.
static constexpr uint32_t STATIC_PASSKEY = 438209;

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  valueConfig.writable = true;
  valueConfig.authenticatedRead = true;
  valueConfig.authenticatedWrite = true;

  auto &gattServer = ble.gattServer();
  gattServer.addService(SERVICE_UUID);
  gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(SERVICE_UUID, CHARACTERISTIC_UUID, String("MITM protected value"));

  EspBleConfig config;
  config.deviceName = "EspBle Passkey";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  config.security.ioCapability = EspBleSecurityIoCapability::DisplayOnly;
  config.security.staticPasskeyEnabled = true;
  config.security.staticPasskey = STATIC_PASSKEY;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onPasskeyDisplayed([](const EspBlePasskeyDisplayed &event) {
    Serial.printf("Enter passkey %06u on the peer.\n", static_cast<unsigned>(event.passkey));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "Security %s: encrypted=%u authenticated=%u bonded=%u\n",
      event.success ? "established" : "failed",
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Passkey");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();
}

void loop()
{
  ble.update();
  delay(1);
}
