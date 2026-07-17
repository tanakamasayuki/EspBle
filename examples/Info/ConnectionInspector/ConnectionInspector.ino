#include <EspBle.h>

// Interactive diagnostic tool: lists nearby connectable devices, connects to
// the one you pick, and dumps the connection snapshot (MTU, roles, security
// state). Also prints the bond store and the library's diagnostic counters.
//
// Security is disabled here, so peripherals that require encryption will
// reject reads but still accept the connection and show their link info.

static constexpr size_t MaxFound = 10;

EspBle ble;
EspBleScanResult found[MaxFound];
size_t foundCount = 0;
EspBleConnectionId currentId = 0;

static bool alreadyListed(const EspBleScanResult &scanResult)
{
  for (size_t i = 0; i < foundCount; ++i)
  {
    if (found[i].address == scanResult.address)
    {
      return true;
    }
  }
  return false;
}

static void printConnection(const EspBleConnection &connection)
{
  Serial.printf(
    "CONNECTION id=%u handle=%u peer=%s(type=%u) role=%s\n"
    "  mtu=%u maxNotificationPayload=%u\n"
    "  encrypted=%u authenticated=%u bonded=%u keySize=%u\n",
    static_cast<unsigned>(connection.id),
    connection.handle,
    connection.peerAddress.c_str(),
    connection.peerAddressType,
    connection.localRole == EspBleRole::Central ? "Central" : "Peripheral",
    connection.mtu,
    static_cast<unsigned>(connection.maximumNotificationPayload()),
    connection.encrypted ? 1 : 0,
    connection.authenticated ? 1 : 0,
    connection.bonded ? 1 : 0,
    connection.encryptionKeySize);
}

static void printBonds()
{
  const size_t count = ble.bondCount();
  Serial.printf("BONDS count=%u\n", static_cast<unsigned>(count));
  for (size_t i = 0; i < count; ++i)
  {
    EspBleBond bond;
    if (ble.bond(i, bond))
    {
      Serial.printf("  [%u] %s type=%u\n", static_cast<unsigned>(i), bond.peerAddress.c_str(), bond.peerAddressType);
    }
  }
}

static void printCounters()
{
  Serial.printf(
    "COUNTERS connections=%u droppedEvents=%u droppedScanResults=%u\n",
    static_cast<unsigned>(ble.connectionCount()),
    static_cast<unsigned>(ble.droppedEventCount()),
    static_cast<unsigned>(ble.scanner().droppedResultCount()));
}

static void startScan()
{
  foundCount = 0;
  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  Serial.printf("SCAN restart success=%u — send the list number to connect\n",
    ble.scanner().start(scanConfig) ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Inspector";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    currentId = connection.id;
    printConnection(connection);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    currentId = 0;
    Serial.printf("DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
    Serial.printf("CONNECT_FAILED peer=%s detail=%s\n", failure.peerAddress.c_str(), failure.detail.c_str());
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (!scanResult.connectable || foundCount >= MaxFound || alreadyListed(scanResult))
    {
      return;
    }
    found[foundCount] = scanResult;
    Serial.printf(
      "[%u] %s rssi=%d%s%s\n",
      static_cast<unsigned>(foundCount),
      scanResult.address.c_str(),
      scanResult.rssi,
      scanResult.hasName() ? " name=" : "",
      scanResult.hasName() ? scanResult.name.c_str() : "");
    ++foundCount;
  });

  Serial.println("Commands: 0-9 connect to listed device, s rescan, d disconnect, b bonds, q counters");
  startScan();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command >= '0' && command <= '9')
    {
      const size_t index = static_cast<size_t>(command - '0');
      if (index < foundCount)
      {
        ble.scanner().stop();
        Serial.printf(
          "CONNECT [%u] %s accepted=%u\n",
          static_cast<unsigned>(index),
          found[index].address.c_str(),
          ble.connect(found[index]) ? 1 : 0);
      }
    }
    else if (command == 's')
    {
      startScan();
    }
    else if (command == 'd')
    {
      Serial.printf("DISCONNECT accepted=%u\n", ble.disconnect(currentId) ? 1 : 0);
    }
    else if (command == 'b')
    {
      printBonds();
    }
    else if (command == 'q')
    {
      printCounters();
    }
  }

  ble.update();
  delay(1);
}
