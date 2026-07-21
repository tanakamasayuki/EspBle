// runtime_passkey DUT: EspBle GATT client acting as the Passkey Entry input
// side (KeyboardOnly, MITM, no static passkey). It connects, pairing begins, and
// the backend's passkey request blocks until the sketch supplies the peer's
// displayed passkey via providePasskey() — driven here by a 'k<passkey>' serial
// command, simulating a user reading the peer's display and typing it in.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *MARKER_SERVICE_UUID = "1815"; // advertised marker

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleConfig config;
  config.deviceName = "EspBle RtPasskey Central";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  config.security.ioCapability = EspBleSecurityIoCapability::KeyboardOnly;
  // No static passkey: the passkey is supplied at runtime via providePasskey().
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("CENTRAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "CENTRAL_SECURITY success=%u encrypted=%u authenticated=%u bonded=%u key=%u context=%s\n",
      event.success ? 1 : 0, event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0, event.connection.bonded ? 1 : 0,
      event.connection.encryptionKeySize, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
    connectionId = 0;
    connectionRequested = false;
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(MARKER_SERVICE_UUID))
      return;
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
    Serial.println(connectionRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("CENTRAL_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 's' && !connectionRequested)
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'k')
    {
      const uint32_t passkey = static_cast<uint32_t>(Serial.parseInt());
      Serial.printf("PASSKEY_PROVIDED accepted=%u passkey=%06u\n",
        ble.providePasskey(passkey) ? 1 : 0, static_cast<unsigned>(passkey));
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
