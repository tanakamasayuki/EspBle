// user_data peer_device: EspBle GATT server for the standard User Data Service.
// Age (0x2A80) is a read/write uint8; First Name (0x2A8A) is a read/write utf8s;
// Database Change Increment (0x2A99) is a read/write/notify uint32. Each write
// to Age or First Name bumps the Database Change Increment and notifies it, so
// the test can observe the client-write -> onWritten -> notify path.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *USER_DATA_SERVICE_UUID = "181c";
static constexpr const char *AGE_UUID = "2a80";
static constexpr const char *FIRST_NAME_UUID = "2a8a";
static constexpr const char *DB_CHANGE_INCREMENT_UUID = "2a99";

EspBle ble;
TaskHandle_t loopTask = nullptr;
uint32_t dbChangeIncrement = 0;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

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
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

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
  if (!server.addService(USER_DATA_SERVICE_UUID) ||
      !server.addCharacteristic(USER_DATA_SERVICE_UUID, AGE_UUID, ageConfig) ||
      !server.addCharacteristic(USER_DATA_SERVICE_UUID, FIRST_NAME_UUID, nameConfig) ||
      !server.addCharacteristic(USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID, incrementConfig) ||
      !server.setValue(USER_DATA_SERVICE_UUID, AGE_UUID, &initialAge, sizeof(initialAge)) ||
      !server.setValue(USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID, initialIncrement, sizeof(initialIncrement)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (write.characteristicUuid.equalsIgnoreCase(AGE_UUID))
    {
      const uint8_t age = write.value.length() == 1 ? static_cast<uint8_t>(write.value[0]) : 0;
      ble.gattServer().setValue(USER_DATA_SERVICE_UUID, AGE_UUID, &age, sizeof(age));
      Serial.printf("SERVER_WRITE char=age value=%u context=%s\n", age, contextName());
      notifyIncrement();
    }
    else if (write.characteristicUuid.equalsIgnoreCase(FIRST_NAME_UUID))
    {
      Serial.printf("SERVER_WRITE char=name length=%u first=%c context=%s\n",
        static_cast<unsigned>(write.value.length()),
        write.value.length() > 0 ? write.value[0] : '?', contextName());
      notifyIncrement();
    }
  });
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("DBCI_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle UserData Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle UserData Peer");
  ble.advertising().addServiceUuid(USER_DATA_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
