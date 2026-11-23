// Peer for the core-host security interoperability test. It links no EspBle
// code: pairing, bonding and the encrypted attribute all come from the BLE
// library Arduino-ESP32 ships, which on the original ESP32 runs on Bluedroid.
// Bonds are removed through BLESecurity, which hands them to Bluedroid's own
// esp_ble_remove_bond_device(), not to a NimBLE store call, because this board
// must stay on its own stack.
#include <BLE.h>
#include <esp_gap_ble_api.h>

#include "../../../sketch_support/EspBleTestLifecycle.h"

static const BLEUUID ServiceUuid("2f7a2000-9d0b-4f6a-9b41-1c8f3a5d0002");
static const BLEUUID SecureUuid("2f7a2001-9d0b-4f6a-9b41-1c8f3a5d0002");

BLEServer server;
BLECharacteristic secureCharacteristic;
bool linkUp = false;
uint16_t connectionId = 0;
bool authenticated = false;
bool bondedFlag = false;
unsigned encryptedReads = 0;
bool stackStopped = false;

static int bondCount()
{
  return static_cast<int>(BLE.getSecurity().getBondedDevices().size());
}

void reportReady()
{
  Serial.printf("SECPEER_READY address=%s\n",
    BLE.getAddress().toString().c_str());
  Serial.printf("SECPEER_STATE connected=%u authenticated=%u bonded=%u bonds=%d reads=%u\n",
    linkUp ? 1 : 0, authenticated ? 1 : 0, bondedFlag ? 1 : 0,
    bondCount(), encryptedReads);
}

// The handlers below run on the BLE stack task.
static void onConnect(BLEServer &, const BLEConnInfo &connection)
{
  linkUp = true;
  connectionId = connection.getHandle();
  Serial.printf("SECPEER_CONNECTED id=%u\n", connectionId);
}

static void onDisconnect(BLEServer &, const BLEConnInfo &, uint8_t)
{
  linkUp = false;
  authenticated = false;
  Serial.println("SECPEER_DISCONNECTED");
  // The server advertises again by itself (advertiseOnDisconnect).
}

static uint32_t onPassKeyRequest(const BLEConnInfo &)
{
  Serial.println("SECPEER_PASSKEY_REQUEST");
  return 123456;
}

static void onPassKeyDisplay(const BLEConnInfo &, uint32_t passKey)
{
  Serial.printf("SECPEER_PASSKEY_NOTIFY value=%06u\n", static_cast<unsigned>(passKey));
}

static bool onSecurityRequest(const BLEConnInfo &)
{
  Serial.println("SECPEER_SECURITY_REQUEST");
  return true;
}

static bool onConfirmPassKey(const BLEConnInfo &, uint32_t pin)
{
  Serial.printf("SECPEER_CONFIRM value=%06u\n", static_cast<unsigned>(pin));
  return true;
}

static void onAuthenticationComplete(const BLEConnInfo &, bool success)
{
  authenticated = success;
  bondedFlag = success;
}

// The library's completion handler does not carry the negotiated auth_mode, so
// the raw Bluedroid event, delivered here after that handler has run, reports
// it. auth_mode carries the LE Secure Connections and bonding bits the peer
// agreed to, so a downgrade shows up here rather than as a silent pass.
static int onGapEvent(void *event, void *arg)
{
  if (event == nullptr || arg == nullptr ||
      *static_cast<esp_gap_ble_cb_event_t *>(event) != ESP_GAP_BLE_AUTH_CMPL_EVT)
  {
    return 0;
  }
  const esp_ble_auth_cmpl_t &result =
    static_cast<esp_ble_gap_cb_param_t *>(arg)->ble_security.auth_cmpl;
  Serial.printf("SECPEER_AUTH success=%u auth_mode=%02x bonds=%d\n",
    result.success ? 1 : 0, result.auth_mode, bondCount());
  return 0;
}

static void onSecureRead(BLECharacteristic &, const BLEConnInfo &)
{
  ++encryptedReads;
  Serial.printf("SECPEER_ENCRYPTED_READ count=%u\n", encryptedReads);
}

void removeAllBonds()
{
  BLESecurity security = BLE.getSecurity();
  const std::vector<BTAddress> bonds = security.getBondedDevices();
  if (bonds.empty())
  {
    Serial.println("SECPEER_BONDS_CLEARED removed=0 remaining=0");
    return;
  }
  unsigned removed = 0;
  for (const BTAddress &address : bonds)
  {
    if (security.deleteBond(address)) ++removed;
  }
  bondedFlag = false;
  // deleteBond() waits for the store update, but keep waiting for the list to
  // drain rather than report a stale number.
  int remaining = bondCount();
  for (unsigned attempt = 0; attempt < 40 && remaining > 0; ++attempt)
  {
    delay(50);
    remaining = bondCount();
  }
  Serial.printf("SECPEER_BONDS_CLEARED removed=%u remaining=%d\n", removed,
    remaining);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLE.begin("EspBle CoreHost Secure"))
  {
    Serial.println("SECPEER_INIT_FAILED");
    return;
  }
  BLESecurity security = BLE.getSecurity();
  security.onPassKeyRequest(onPassKeyRequest);
  security.onPassKeyDisplay(onPassKeyDisplay);
  security.onSecurityRequest(onSecurityRequest);
  security.onConfirmPassKey(onConfirmPassKey);
  security.onAuthenticationComplete(onAuthenticationComplete);
  // Bonding and LE Secure Connections without MITM (ESP_LE_AUTH_REQ_SC_BOND).
  security.setAuthenticationMode(true, false, true);
  security.setIOCapability(BLEIOCapability::NoInputNoOutput);
  security.setInitiatorKeys(BLESecurity::KeyDist::EncKey | BLESecurity::KeyDist::IdKey);
  security.setResponderKeys(BLESecurity::KeyDist::EncKey | BLESecurity::KeyDist::IdKey);
  BLE.setCustomGapHandler(onGapEvent);

  server = BLE.createServer();
  server.onConnect(onConnect);
  server.onDisconnect(onDisconnect);
  server.advertiseOnDisconnect(true);

  BLEService service = server.createService(ServiceUuid, 20);
  // "Encryption required" is an attribute permission, so an unencrypted read
  // must fail at the ATT layer.
  secureCharacteristic = service.createCharacteristic(
    SecureUuid,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermission::ReadWriteEncrypted);
  secureCharacteristic.onRead(onSecureRead);
  secureCharacteristic.setValue("core-host-secret");
  if (!server.start())
  {
    Serial.println("SECPEER_INIT_FAILED");
    return;
  }

  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.addServiceUUID(ServiceUuid);
  advertising.setScanResponse(true);
  if (!BLE.startAdvertising())
  {
    Serial.println("SECPEER_INIT_FAILED");
    return;
  }

  reportReady();

  // onDisconnect advertises again on its own, so RECOVER only drops the link.
  // end() takes the radio down synchronously, so STOP is complete at once.
  EspBleTestLifecycle::Hooks hooks;
  hooks.stop = []() {
    stackStopped = true;
    BLE.end(false);
  };
  EspBleTestLifecycle::begin(hooks);
}

void loop()
{
  EspBleTestLifecycle::update();
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (EspBleTestLifecycle::handleLine(command)) return;
    // STOP has taken the stack down; the commands have nothing left to act on.
    if (stackStopped) return;
    if (command == "?")
    {
      reportReady();
    }
    else if (command == "c")
    {
      removeAllBonds();
    }
    else if (command == "d" && linkUp)
    {
      server.disconnect(connectionId);
      Serial.println("SECPEER_DISCONNECT requested=1");
    }
  }
  delay(10);
}
