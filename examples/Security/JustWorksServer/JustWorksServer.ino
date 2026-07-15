#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "be31dd60-5e70-4fd5-9003-736563757265";
static constexpr const char *CHARACTERISTIC_UUID = "be31dd61-5e70-4fd5-9003-736563757265";

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  valueConfig.writable = true;
  valueConfig.encryptedRead = true;
  valueConfig.encryptedWrite = true;

  auto &gattServer = ble.gattServer();
  gattServer.addService(SERVICE_UUID);
  gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(SERVICE_UUID, CHARACTERISTIC_UUID, String("encrypted value"));
  gattServer.onWritten([](const EspBleGattWrite &write) {
    Serial.printf("Encrypted write: %s\n", write.value.c_str());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Secure";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.pairOnConnect = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "Security %s: encrypted=%u authenticated=%u bonded=%u keySize=%u\n",
      event.success ? "established" : "failed",
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      event.connection.encryptionKeySize);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Secure");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();

  Serial.println("Send 'c' while disconnected to clear all bonds.");
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 'c')
  {
    Serial.printf(
      "Clear bonds: %s, remaining=%u\n",
      ble.deleteAllBonds() ? "success" : ble.lastErrorName(),
      static_cast<unsigned>(ble.bondCount()));
  }

  ble.update();
  delay(1);
}
