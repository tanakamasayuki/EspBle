// Central for the accept_list peer test: scan for the peripheral and try to
// connect. The connection must fail while the peripheral filters connections
// against its accept list, and succeed once the policy is open again.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAD";
// Shorter than the library default so a blocked attempt reports back quickly.
static constexpr uint32_t CONNECT_TIMEOUT_MS = 4000;

EspBle ble;
bool connectRequested = false;

// What the current scan is for: connecting, or checking which advertisers the
// controller lets through while the scanner filters against the accept list.
enum class ScanMode
{
  Connect,
  Observe,
};
ScanMode scanMode = ScanMode::Connect;
bool targetSeen = false;
String targetAddress;
EspBleAddressType targetAddressType = EspBleAddressType::Public;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Accept List Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (!scanResult.advertisesService(SERVICE_UUID)) return;
    if (scanMode == ScanMode::Observe)
    {
      targetSeen = true;
      targetAddress = scanResult.address;
      targetAddressType = scanResult.addressType;
      return;
    }
    if (connectRequested) return;
    connectRequested = true;
    ble.scanner().stop();
    Serial.printf("TARGET_FOUND %s\n", scanResult.address.c_str());
    if (!ble.connect(scanResult, CONNECT_TIMEOUT_MS))
    {
      Serial.printf("CONNECT_REJECTED %s\n", ble.lastErrorName());
    }
  });

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_CONNECTED id=%u\n", connection.id);
  });
  ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
    Serial.printf("CENTRAL_CONNECT_FAILED %s\n", failure.detail.c_str());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_DISCONNECTED id=%u\n", connection.id);
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's' || command == 'f')
    {
      // 's' observes every advertiser; 'f' only those on the accept list.
      scanMode = ScanMode::Observe;
      targetSeen = false;
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      scanConfig.acceptListOnly = command == 'f';
      Serial.println(ble.scanner().start(scanConfig) ? "OBSERVE_STARTED" : "OBSERVE_START_FAILED");
    }
    else if (command == 'n')
    {
      ble.scanner().stop();
      Serial.printf("OBSERVED target=%u address=%s\n", targetSeen ? 1 : 0, targetAddress.c_str());
    }
    else if (command == 'a')
    {
      const bool added = targetAddress.length() > 0 &&
        ble.addToAcceptList(targetAddress.c_str(), targetAddressType);
      Serial.printf("CENTRAL_ACCEPT_LIST added=%u count=%u\n",
        added ? 1 : 0, static_cast<unsigned>(ble.acceptListCount()));
    }
    else if (command == 'e')
    {
      // Read the list back through the public accessor, so the entries the
      // controller holds are observable and not just implied by the filtering.
      Serial.printf("CENTRAL_ACCEPT_LIST_DUMP count=%u\n",
        static_cast<unsigned>(ble.acceptListCount()));
      for (size_t index = 0; index < ble.acceptListCount(); ++index)
      {
        EspBleBond entry;
        if (!ble.acceptListEntry(index, entry))
        {
          Serial.printf("CENTRAL_ACCEPT_LIST_ENTRY index=%u error=1\n",
            static_cast<unsigned>(index));
          continue;
        }
        Serial.printf("CENTRAL_ACCEPT_LIST_ENTRY index=%u address=%s type=%u\n",
          static_cast<unsigned>(index),
          entry.peerAddress.c_str(),
          static_cast<unsigned>(entry.peerAddressType));
      }
    }
    else if (command == 'm')
    {
      const bool removed = targetAddress.length() > 0 &&
        ble.removeFromAcceptList(targetAddress.c_str(), targetAddressType);
      Serial.printf("CENTRAL_ACCEPT_LIST removed=%u count=%u\n",
        removed ? 1 : 0, static_cast<unsigned>(ble.acceptListCount()));
    }
    else if (command == 'x')
    {
      ble.clearAcceptList();
      Serial.printf("CENTRAL_ACCEPT_LIST added=0 count=%u\n",
        static_cast<unsigned>(ble.acceptListCount()));
    }
    else if (command == 'c')
    {
      scanMode = ScanMode::Connect;
      connectRequested = false;
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'd')
    {
      Serial.println(ble.disconnect(1) ? "DISCONNECT_REQUESTED" : "DISCONNECT_FAILED");
    }
  }

  ble.update();
  delay(1);
}
