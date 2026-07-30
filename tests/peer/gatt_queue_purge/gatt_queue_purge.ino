// gatt_queue_purge DUT: the Central. It checks the two things that happen when a
// connection goes away with GATT work outstanding.
//
// 1. disconnect() during an in-flight GATT operation is DEFERRED, not rejected.
//    The operation finishes normally and the disconnect happens afterwards. The
//    old behaviour was to return false, which reads to the application as "still
//    connected" when it had asked to disconnect.
// 2. Queued-but-unstarted operations for that connection are PURGED, and each one
//    still gets a failure completion. Dropping them silently would leave the
//    application waiting for callbacks that can never arrive.
//
// Both come out of one sequence: queue four reads (the first goes on the air, the
// rest sit in the queue), then ask to disconnect immediately. Expected order is
// the first read's success, the disconnect, then a failure for each of the three
// that never started.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "6b1d0000-9c4e-4a71-8f2d-3e5a7c9b1000";
static const char *CHARACTERISTIC_UUIDS[] = {
  "6b1d0001-9c4e-4a71-8f2d-3e5a7c9b1000",
  "6b1d0002-9c4e-4a71-8f2d-3e5a7c9b1000",
  "6b1d0003-9c4e-4a71-8f2d-3e5a7c9b1000",
  "6b1d0004-9c4e-4a71-8f2d-3e5a7c9b1000",
};
static constexpr size_t CharacteristicCount =
  sizeof(CHARACTERISTIC_UUIDS) / sizeof(CHARACTERISTIC_UUIDS[0]);

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

// The last hex digit of the characteristic UUID identifies which read this was.
static char reportTag(const String &uuid)
{
  const int dash = uuid.indexOf('-');
  return dash > 0 ? uuid.charAt(dash - 1) : '?';
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
    Serial.printf("CENTRAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    ble.discoverServices(connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("CENTRAL_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    Serial.printf("DISCOVERED success=%u characteristics=%u\n",
      result.success ? 1 : 0,
      static_cast<unsigned>(
        ble.discoveredCharacteristicCount(result.connectionId, SERVICE_UUID)));
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    // EspBleError has no public name helper (lastErrorName() covers the
    // synchronous path only), so the code is printed: 1 = InvalidState.
    Serial.printf("READ tag=%c success=%u error=%u value=%s detail=%s\n",
      reportTag(result.characteristicUuid),
      result.success ? 1 : 0,
      static_cast<unsigned>(result.error),
      result.value.c_str(),
      result.detail.c_str());
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
    else if (command == 'q' && connectionId != 0)
    {
      // Four reads: the first is pumped onto the air right away, the other three
      // wait in the queue. Kept small on purpose — the connection event queue
      // holds 8, and the purge pushes one disconnect plus one failure per queued
      // operation, so a larger batch would start dropping the very events this
      // test is here to observe.
      size_t queued = 0;
      for (size_t index = 0; index < CharacteristicCount; ++index)
      {
        if (ble.readCharacteristic(connectionId, SERVICE_UUID, CHARACTERISTIC_UUIDS[index]))
          ++queued;
      }
      // Asked for while the first read is still in flight. This must be accepted
      // (deferred), not refused.
      const bool accepted = ble.disconnect(connectionId);
      Serial.printf("PURGE_SETUP queued=%u disconnect=%u error=%s\n",
        static_cast<unsigned>(queued), accepted ? 1 : 0, ble.lastErrorName());
    }
    else if (command == 'c')
    {
      // Nothing may have been lost to the event queue; a drop here would mean the
      // assertions above were reading an incomplete picture.
      Serial.printf("DROPPED events=%u\n",
        static_cast<unsigned>(ble.droppedEventCount()));
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId)
        ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
