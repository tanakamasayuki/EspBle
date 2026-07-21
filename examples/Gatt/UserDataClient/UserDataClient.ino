// en: UserDataClient - connect to a User Data Service (0x181C), subscribe to
//     Database Change Increment notifications, read Age, and write a new First
//     Name and Age. Each write bumps the increment, which arrives as a
//     notification.
// ja: UserDataClient - User Data Service（0x181C）へ接続し、Database Change
//     IncrementのNotificationを購読、AgeをRead、新しいFirst NameとAgeをWriteする。
//     書き込むたびにincrementが増え、Notificationとして届く。
#include <EspBle.h>

static constexpr const char *USER_DATA_SERVICE_UUID = "181c";
static constexpr const char *AGE_UUID = "2a80";
static constexpr const char *FIRST_NAME_UUID = "2a8a";
static constexpr const char *DB_CHANGE_INCREMENT_UUID = "2a99";

EspBle ble;
EspBleConnectionId connectionId = 0;
bool wroteProfile = false;

void setup()
{
  Serial.begin(115200);
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    wroteProfile = false;
    ble.subscribe(connection.id, USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID);
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.success)
      ble.readCharacteristic(result.connectionId, USER_DATA_SERVICE_UUID, AGE_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(AGE_UUID) && result.success && result.value.length() == 1)
    {
      Serial.printf("Age: %u\n", static_cast<uint8_t>(result.value[0]));
      // en: Write a new profile once, right after reading the current Age.
      // ja: 現在のAgeを読んだ直後に、一度だけ新しいprofileをWriteする。
      if (!wroteProfile)
      {
        wroteProfile = true;
        const uint8_t name[3] = {'A', 'd', 'a'};
        ble.writeCharacteristic(result.connectionId, USER_DATA_SERVICE_UUID, FIRST_NAME_UUID, name, sizeof(name), true);
        const uint8_t age = 42;
        ble.writeCharacteristic(result.connectionId, USER_DATA_SERVICE_UUID, AGE_UUID, &age, sizeof(age), true);
      }
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(DB_CHANGE_INCREMENT_UUID) ||
        notification.value.length() != 4)
      return;
    uint32_t increment = 0;
    for (int i = 3; i >= 0; --i)
      increment = (increment << 8) | static_cast<uint8_t>(notification.value[i]);
    Serial.printf("Database Change Increment: %u\n", static_cast<unsigned>(increment));
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(USER_DATA_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  ble.update();
  delay(1);
}
