// persistent_subscription_overflow DUT: the Central. It fills the persistent
// subscription registry (16 records, keyed by peer address + service +
// characteristic) past its capacity and checks that the overflow is counted
// rather than silently dropped.
//
// Route: subscribe to 12 characteristics on the peer's public address, disconnect
// (which frees the active subscription table but keeps the records), let the peer
// re-init as a random static address, then subscribe to 5 of the same
// characteristics again. The new address makes them new records, so the 17th one
// finds the registry full and droppedPersistentSubscriptionCount() reports 1.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "2f9b0000-3a71-4d1e-9c3f-8a5d6e7f1000";
static const char *CHARACTERISTIC_UUIDS[] = {
  "2f9b0001-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0002-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0003-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0004-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0005-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0006-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0007-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0008-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b0009-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b000a-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b000b-3a71-4d1e-9c3f-8a5d6e7f1000",
  "2f9b000c-3a71-4d1e-9c3f-8a5d6e7f1000",
};
static constexpr size_t CharacteristicCount =
  sizeof(CHARACTERISTIC_UUIDS) / sizeof(CHARACTERISTIC_UUIDS[0]);

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
String connectedAddress;

// Per-batch subscribe accounting. Subscribes are issued one at a time from
// loop(): the GATT operation queue holds 8 entries beside the one in flight, so
// firing a dozen at once would be refused with ResourceExhausted before any of
// them reached the air. Serialising them keeps the batch about the registry
// rather than about the queue.
size_t batchTarget = 0;
size_t batchIssued = 0;
size_t batchSucceeded = 0;
size_t batchFailed = 0;
bool batchActive = false;
bool awaitingResult = false;

static void startBatch(size_t count)
{
  batchTarget = count;
  batchIssued = 0;
  batchSucceeded = 0;
  batchFailed = 0;
  batchActive = true;
  awaitingResult = false;
  Serial.printf("BATCH_STARTED count=%u\n", static_cast<unsigned>(count));
}

static void pumpBatch()
{
  if (!batchActive || awaitingResult || batchIssued >= batchTarget) return;
  if (ble.subscribe(
        connectionId, SERVICE_UUID, CHARACTERISTIC_UUIDS[batchIssued], true))
  {
    ++batchIssued;
    awaitingResult = true;
    return;
  }
  Serial.printf("SUBSCRIBE_REJECTED index=%u error=%s\n",
    static_cast<unsigned>(batchIssued), ble.lastErrorName());
  batchActive = false;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  if (!ble.begin())
  {
    Serial.printf("INIT_FAILED %s %s\n",
      ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    connectedAddress = connection.peerAddress;
    Serial.printf("CENTRAL_CONNECTED id=%u\n",
      static_cast<unsigned>(connection.id));
    ble.discoverServices(connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("CENTRAL_DISCONNECTED id=%u\n",
      static_cast<unsigned>(connection.id));
  });
  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    Serial.printf("DISCOVERED success=%u characteristics=%u\n",
      result.success ? 1 : 0,
      static_cast<unsigned>(
        ble.discoveredCharacteristicCount(result.connectionId, SERVICE_UUID)));
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    awaitingResult = false;
    if (result.success) ++batchSucceeded;
    else ++batchFailed;
    if (batchSucceeded + batchFailed < batchTarget) return;
    batchActive = false;
    // The registry is consulted when a subscribe succeeds, so the counter is
    // only meaningful once the whole batch has landed.
    Serial.printf("BATCH_DONE subscribed=%u failed=%u dropped=%u\n",
      static_cast<unsigned>(batchSucceeded),
      static_cast<unsigned>(batchFailed),
      static_cast<unsigned>(ble.droppedPersistentSubscriptionCount()));
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
    Serial.println(connectionRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's' && !connectionRequested)
    {
      EspBleScanConfig scan;
      scan.active = true;
      Serial.println(ble.scanner().start(scan) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'a')
    {
      Serial.printf("PEER_ADDRESS %s\n", connectedAddress.c_str());
    }
    else if (command == '1' && connectionId != 0)
    {
      // All 12: exactly the server's CCCD tracking capacity, and four short of
      // the central's own 16-entry active table.
      startBatch(CharacteristicCount);
    }
    else if (command == '2' && connectionId != 0)
    {
      // Five more under the new address: records 13-16 fit, the 17th does not.
      startBatch(5);
    }
    else if (command == 'c')
    {
      Serial.printf("DROPPED count=%u\n",
        static_cast<unsigned>(ble.droppedPersistentSubscriptionCount()));
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId)
        ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  pumpBatch();
  ble.update();
  delay(1);
}
