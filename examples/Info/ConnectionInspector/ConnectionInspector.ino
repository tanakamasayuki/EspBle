// en: Interactively connect and dump MTU, security state, bonds, counters
// ja: 対話式に接続してMTU・security状態・Bond・カウンタをダンプ
#include <EspBle.h>

static constexpr size_t MaxFound = 10;

EspBle ble;
EspBleScanResult found[MaxFound]; // en: listed devices / ja: 一覧した機器
size_t foundCount = 0;
EspBleConnectionId currentId = 0;

// en: Has this address already been listed?
// ja: このaddressは既に一覧済みか。
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

// en: Dump the full connection snapshot.
// ja: Connection snapshotをすべてダンプする。
static void printConnection(const EspBleConnection &connection)
{
  Serial.printf(
    "CONNECTION id=%u handle=%u peer=%s(type=%u) role=%s\n"
    "  mtu=%u maxNotificationPayload=%u\n"
    "  encrypted=%u authenticated=%u bonded=%u keySize=%u\n",
    static_cast<unsigned>(connection.id),
    connection.handle,
    connection.peerAddress.c_str(),
    static_cast<unsigned>(connection.peerAddressType),
    connection.localRole == EspBleRole::Central ? "Central" : "Peripheral",
    connection.mtu,
    static_cast<unsigned>(connection.maximumNotificationPayload()),
    connection.encrypted ? 1 : 0,
    connection.authenticated ? 1 : 0,
    connection.bonded ? 1 : 0,
    connection.encryptionKeySize);
}

// en: Dump the bond store (snapshot index access).
// ja: Bond storeをダンプする（snapshot indexアクセス）。
static void printBonds()
{
  const size_t count = ble.bondCount();
  Serial.printf("BONDS count=%u\n", static_cast<unsigned>(count));
  for (size_t i = 0; i < count; ++i)
  {
    EspBleBond bond;
    if (ble.bond(i, bond))
    {
      Serial.printf("  [%u] %s type=%u\n", static_cast<unsigned>(i), bond.peerAddress.c_str(),
        static_cast<unsigned>(bond.peerAddressType));
    }
  }
}

// en: Print library diagnostic counters.
// ja: ライブラリの診断カウンタを表示する。
static void printCounters()
{
  Serial.printf(
    "COUNTERS connections=%u droppedEvents=%u droppedScanResults=%u\n",
    static_cast<unsigned>(ble.connectionCount()),
    static_cast<unsigned>(ble.droppedEventCount()),
    static_cast<unsigned>(ble.scanner().droppedResultCount()));
}

// en: Clear the list and restart scanning.
// ja: 一覧をクリアして再scanする。
static void startScan()
{
  foundCount = 0;
  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  Serial.printf("SCAN restart success=%u - send the list number to connect\n",
    ble.scanner().start(scanConfig) ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Inspector";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  // en: On connect, remember the id and dump the snapshot.
  // ja: 接続したらidを覚えてsnapshotをダンプする。
  ble.onConnected([](const EspBleConnection &connection) {
    currentId = connection.id;
    printConnection(connection);
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &connection) {
    currentId = 0;
    Serial.printf("DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
    Serial.printf("CONNECT_FAILED peer=%s detail=%s\n", failure.peerAddress.c_str(), failure.detail.c_str());
  });
  // en: List up to MaxFound unique connectable devices as [index] address rssi name.
  // ja: connectableな機器を最大MaxFound件、[index] address rssi name 形式で一覧する。
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
      // en: connect to the listed device with that index
      // ja: その番号の一覧機器へ接続する
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
      startScan(); // en: rescan / ja: 再scan
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

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
