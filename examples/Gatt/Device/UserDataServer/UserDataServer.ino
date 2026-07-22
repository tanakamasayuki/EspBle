// en: UserDataServer - standard User Data Service (0x181C). Age (0x2A80) is a
//     read/write uint8, First Name (0x2A8A) is a read/write utf8s, and Database
//     Change Increment (0x2A99) is a read/write/notify uint32. Each write to Age
//     or First Name bumps the increment and notifies it.
// ja: UserDataServer - 標準User Data Service（0x181C）。Age（0x2A80）はread/write
//     のuint8、First Name（0x2A8A）はread/writeのutf8s、Database Change Increment
//     （0x2A99）はread/write/notifyのuint32。AgeかFirst Nameが書かれるたびに
//     incrementを増やしてNotifyする。
#include <EspBle.h>

static constexpr const char *USER_DATA_SERVICE_UUID = "181c";
static constexpr const char *AGE_UUID = "2a80";
static constexpr const char *FIRST_NAME_UUID = "2a8a";
static constexpr const char *DB_CHANGE_INCREMENT_UUID = "2a99";

EspBle ble;
uint32_t dbChangeIncrement = 0;

static void notifyIncrement()
{
  ++dbChangeIncrement;
  uint8_t bytes[4];
  bytes[0] = static_cast<uint8_t>(dbChangeIncrement & 0xFF);
  bytes[1] = static_cast<uint8_t>((dbChangeIncrement >> 8) & 0xFF);
  bytes[2] = static_cast<uint8_t>((dbChangeIncrement >> 16) & 0xFF);
  bytes[3] = static_cast<uint8_t>((dbChangeIncrement >> 24) & 0xFF);
  ble.gattServer().setValue(USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID, bytes, sizeof(bytes));
  ble.gattServer().notify(USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID, bytes, sizeof(bytes));
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig ageConfig;
  ageConfig.readable = true;
  ageConfig.writable = true;
  EspBleGattCharacteristicConfig nameConfig;
  nameConfig.readable = true;
  nameConfig.writable = true;
  EspBleGattCharacteristicConfig incrementConfig;
  incrementConfig.readable = true;
  incrementConfig.writable = true;
  incrementConfig.notifiable = true;
  auto &server = ble.gattServer();
  const uint8_t initialAge = 25;
  const uint8_t initialIncrement[4] = {0, 0, 0, 0};
  server.addService(USER_DATA_SERVICE_UUID);
  server.addCharacteristic(USER_DATA_SERVICE_UUID, AGE_UUID, ageConfig);
  server.addCharacteristic(USER_DATA_SERVICE_UUID, FIRST_NAME_UUID, nameConfig);
  server.addCharacteristic(USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID, incrementConfig);
  server.setValue(USER_DATA_SERVICE_UUID, AGE_UUID, &initialAge, sizeof(initialAge));
  server.setValue(USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID, initialIncrement, sizeof(initialIncrement));

  server.onWritten([](const EspBleGattWrite &write) {
    if (write.characteristicUuid.equalsIgnoreCase(AGE_UUID) && write.value.length() == 1)
    {
      const uint8_t age = static_cast<uint8_t>(write.value[0]);
      ble.gattServer().setValue(USER_DATA_SERVICE_UUID, AGE_UUID, &age, sizeof(age));
      Serial.printf("Age updated: %u\n", age);
      notifyIncrement();
    }
    else if (write.characteristicUuid.equalsIgnoreCase(FIRST_NAME_UUID))
    {
      Serial.printf("First Name updated: %s\n", write.value.c_str());
      notifyIncrement();
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle User Data";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(USER_DATA_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  ble.update();
  delay(1);
}
