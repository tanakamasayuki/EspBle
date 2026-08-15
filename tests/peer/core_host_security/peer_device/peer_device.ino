// Peer for the core-host security interoperability test. It links no EspBle
// code: pairing, bonding and the encrypted attribute all come from the BLE
// wrapper Arduino-ESP32 ships plus the Bluedroid GAP API. Bonds are removed
// with esp_ble_remove_bond_device(), not with a NimBLE store call, because this
// board must stay on its own stack.
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_gap_ble_api.h>

static const char *ServiceUuid = "2f7a2000-9d0b-4f6a-9b41-1c8f3a5d0002";
static const char *SecureUuid = "2f7a2001-9d0b-4f6a-9b41-1c8f3a5d0002";

BLEServer *server = nullptr;
BLECharacteristic *secureCharacteristic = nullptr;
bool linkUp = false;
uint16_t connectionId = 0;
bool authenticated = false;
bool bondedFlag = false;
unsigned encryptedReads = 0;

void reportReady()
{
  Serial.printf("SECPEER_READY address=%s\n",
    BLEDevice::getAddress().toString().c_str());
  Serial.printf("SECPEER_STATE connected=%u authenticated=%u bonded=%u bonds=%d reads=%u\n",
    linkUp ? 1 : 0, authenticated ? 1 : 0, bondedFlag ? 1 : 0,
    esp_ble_get_bond_device_num(), encryptedReads);
}

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *serverInstance, esp_ble_gatts_cb_param_t *param) override
  {
    linkUp = true;
    connectionId = param->connect.conn_id;
    Serial.printf("SECPEER_CONNECTED id=%u\n", connectionId);
  }

  void onDisconnect(BLEServer *serverInstance) override
  {
    linkUp = false;
    authenticated = false;
    Serial.println("SECPEER_DISCONNECTED");
    BLEDevice::startAdvertising();
  }
};

class SecurityCallbacks : public BLESecurityCallbacks
{
  uint32_t onPassKeyRequest() override
  {
    Serial.println("SECPEER_PASSKEY_REQUEST");
    return 123456;
  }

  void onPassKeyNotify(uint32_t passKey) override
  {
    Serial.printf("SECPEER_PASSKEY_NOTIFY value=%06u\n", passKey);
  }

  bool onSecurityRequest() override
  {
    Serial.println("SECPEER_SECURITY_REQUEST");
    return true;
  }

  bool onConfirmPIN(uint32_t pin) override
  {
    Serial.printf("SECPEER_CONFIRM value=%06u\n", pin);
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override
  {
    authenticated = result.success;
    bondedFlag = result.success;
    // auth_mode carries the LE Secure Connections and bonding bits the peer
    // agreed to, so a downgrade shows up here rather than as a silent pass.
    Serial.printf("SECPEER_AUTH success=%u auth_mode=%02x bonds=%d\n",
      result.success ? 1 : 0, result.auth_mode, esp_ble_get_bond_device_num());
  }
};

class SecureCallbacks : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *characteristic) override
  {
    ++encryptedReads;
    Serial.printf("SECPEER_ENCRYPTED_READ count=%u\n", encryptedReads);
  }
};

void removeAllBonds()
{
  const int count = esp_ble_get_bond_device_num();
  if (count <= 0)
  {
    Serial.println("SECPEER_BONDS_CLEARED removed=0 remaining=0");
    return;
  }
  esp_ble_bond_dev_t *list =
    static_cast<esp_ble_bond_dev_t *>(malloc(sizeof(esp_ble_bond_dev_t) * count));
  if (list == nullptr)
  {
    Serial.println("SECPEER_BONDS_CLEARED removed=0 remaining=-1");
    return;
  }
  int total = count;
  esp_ble_get_bond_device_list(&total, list);
  unsigned removed = 0;
  for (int index = 0; index < total; ++index)
  {
    if (esp_ble_remove_bond_device(list[index].bd_addr) == ESP_OK) ++removed;
  }
  free(list);
  bondedFlag = false;
  Serial.printf("SECPEER_BONDS_CLEARED removed=%u remaining=%d\n", removed,
    esp_ble_get_bond_device_num());
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  BLEDevice::init("EspBle CoreHost Secure");
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());
  BLESecurity::setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  BLESecurity::setCapability(ESP_IO_CAP_NONE);
  BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLESecurity::setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(BLEUUID(ServiceUuid), 20);
  secureCharacteristic = service->createCharacteristic(
    BLEUUID(SecureUuid),
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  // Bluedroid expresses "encryption required" as an attribute permission, not
  // as a property, so an unencrypted read must fail at the ATT layer.
  secureCharacteristic->setAccessPermissions(
    ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  secureCharacteristic->setCallbacks(new SecureCallbacks());
  secureCharacteristic->setValue("core-host-secret");
  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLEUUID(ServiceUuid));
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  reportReady();
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
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
      server->disconnect(connectionId);
      Serial.println("SECPEER_DISCONNECT requested=1");
    }
  }
  delay(10);
}
