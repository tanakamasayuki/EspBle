#include <EspBleClassic.h>
#include <EspBle.h>
#include <EspBleHciBroker.h>
#if defined(ESPBLE_TEST_DUAL_RPA)
#include <nimble_esp32/include/host/ble_hs_pvcy.h>
#endif

EspBleClassic classic;
EspBle ble;
bool bleConnectionRequested;
#if defined(ESPBLE_TEST_DUAL_RPA)
bool rpaObserveOnly;
#endif
EspBleConnectionId bleConnectionId;
static const char *ServiceUuid = "c8a53600-98f4-4f2c-a231-522b5c4d9001";
static const char *CharacteristicUuid = "c8a53601-98f4-4f2c-a231-522b5c4d9001";

void printHex(const String &value)
{
  for (size_t i = 0; i < value.length(); ++i)
    Serial.printf("%02x", static_cast<uint8_t>(value[i]));
}

void printHeap(const char *prefix)
{
  Serial.printf("%s free=%u min=%u largest=%u\n", prefix,
    ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
}

bool startClassicStack()
{
  EspBleClassicConfig config;
  config.deviceName = "EspBle Dual Peer";
  return classic.begin(config) && classic.hidHost().begin();
}

bool startDualStacks()
{
  if (!startClassicStack()) return false;
  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle Dual Peer";
  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;
  bleConfig.security.pairOnConnect = true;
#if defined(ESPBLE_TEST_DUAL_RPA)
  bleConfig.ownAddressType = EspBleOwnAddressType::ResolvablePrivate;
#endif
  return ble.begin(bleConfig);
}

bool runDestructorCycle(bool classicFirst)
{
  EspBleClassic *temporaryClassic = new EspBleClassic();
  EspBle *temporaryBle = new EspBle();
  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "EspBle Peer Destructor";
  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle Peer Destructor";
  const bool started = temporaryClassic->begin(classicConfig) &&
    temporaryBle->begin(bleConfig);
  bool survivor = false;
  if (classicFirst)
  {
    delete temporaryClassic;
    survivor = temporaryBle->initialized();
    delete temporaryBle;
  }
  else
  {
    delete temporaryBle;
    survivor = temporaryClassic->initialized();
    delete temporaryClassic;
  }
  return started && survivor;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  classic.hidHost().onConnected([](const EspBleClassicHidConnection &) {
    Serial.println("DUAL_PEER_CONNECTED");
  });
  classic.hidHost().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("DUAL_PEER_DISCONNECTED");
  });
  classic.hidHost().onInputReport([](const EspBleClassicHidReport &report) {
    Serial.printf("DUAL_PEER_INPUT hex=");
    printHex(report.value);
    Serial.println();
    const uint8_t output[] = {0x02, 0xa5, 0x00, 0xff};
    Serial.printf("DUAL_PEER_OUTPUT %u\n",
      classic.hidHost().sendOutputReport(output, sizeof(output)) ? 1 : 0);
  });
  if (!startDualStacks())
  {
    Serial.printf("DUAL_PEER_START_FAILED classic=%s ble=%s\n",
      classic.lastErrorDetail().c_str(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (bleConnectionRequested || !result.advertisesService(ServiceUuid)) return;
#if defined(ESPBLE_TEST_DUAL_RPA)
    Serial.printf("RPA_DUAL_SEEN addr=%s type=%u\n",
      result.address.c_str(), static_cast<unsigned>(result.addressType));
    if (rpaObserveOnly)
    {
      rpaObserveOnly = false;
      ble.scanner().stop();
      Serial.printf("RPA_DUAL_OBSERVED addr=%s type=%u\n",
        result.address.c_str(), static_cast<unsigned>(result.addressType));
      return;
    }
#endif
    ble.scanner().stop();
    bleConnectionRequested = ble.connect(result);
    Serial.printf("DUAL_BLE_CONNECT %u\n", bleConnectionRequested ? 1 : 0);
  });
  ble.onConnected([](const EspBleConnection &connection) {
    bleConnectionId = connection.id;
    Serial.println("DUAL_BLE_CLIENT_CONNECTED");
#if defined(ESPBLE_TEST_DUAL_RPA)
    Serial.printf("RPA_DUAL_CLIENT_PEER addr=%s type=%u\n",
      connection.peerAddress.c_str(),
      static_cast<unsigned>(connection.peerAddressType));
#endif
    Serial.printf("DUAL_BLE_READ_REQUESTED %u\n",
      ble.readCharacteristic(
        connection.id, ServiceUuid, CharacteristicUuid) ? 1 : 0);
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "DUAL_BLE_CLIENT_SECURITY success=%u encrypted=%u bonded=%u key=%u classic=%u\n",
      event.success ? 1 : 0, event.connection.encrypted ? 1 : 0,
      event.connection.bonded ? 1 : 0, event.connection.encryptionKeySize,
      classic.hidHost().connected() ? 1 : 0);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    bleConnectionId = 0;
    bleConnectionRequested = false;
    Serial.println("DUAL_BLE_CLIENT_DISCONNECTED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf("DUAL_BLE_READ success=%u value=%s classic=%u\n",
      result.success ? 1 : 0, result.value.c_str(),
      classic.hidHost().connected() ? 1 : 0);
  });
#if !defined(ESPBLE_TEST_DUAL_RPA)
  (void)ble.deleteAllBonds();
#endif
  Serial.println("DUAL_PEER_READY");
}

void loop()
{
  classic.update();
  ble.update();
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.startsWith("c"))
      Serial.printf("DUAL_PEER_CONNECT %u\n",
        classic.hidHost().connect(command.c_str() + 1) ? 1 : 0);
    else if (command.startsWith("b"))
    {
      bleConnectionRequested = ble.connect(
        command.c_str() + 1, EspBleAddressType::Public);
      Serial.printf("DUAL_BLE_CONNECT %u\n", bleConnectionRequested ? 1 : 0);
    }
    else if (command == "r")
      Serial.printf("DUAL_BLE_READ_REQUESTED %u\n",
        bleConnectionId != 0 &&
        ble.readCharacteristic(bleConnectionId, ServiceUuid, CharacteristicUuid)
          ? 1 : 0);
    else if (command == "g")
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.printf("DUAL_BLE_SCAN %u\n",
        ble.scanner().start(scanConfig) ? 1 : 0);
    }
    else if (command == "k")
      Serial.printf("DUAL_BLE_DISCONNECT %u\n",
        bleConnectionId != 0 && ble.disconnect(bleConnectionId) ? 1 : 0);
    else if (command == "q")
    {
      const bool bleRequested = bleConnectionId != 0 &&
        ble.disconnect(bleConnectionId);
      const bool classicRequested = classic.hidHost().disconnect();
      Serial.printf("DUAL_PEER_DUAL_DISCONNECT ble=%u classic=%u\n",
        bleRequested ? 1 : 0, classicRequested ? 1 : 0);
    }
    else if (command == "n")
      Serial.printf("DUAL_BLE_BONDS %u\n",
        static_cast<unsigned>(ble.bondCount()));
    else if (command == "h")
      printHeap("DUAL_PEER_HEAP");
#if defined(ESPBLE_TEST_DUAL_RPA)
    else if (command == "X")
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("RPA_DUAL_PEER_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0,
        static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == "Z")
    {
      Serial.println("RPA_DUAL_PEER_RESTARTING");
      Serial.flush();
      ESP.restart();
    }
    else if (command == "p")
    {
      rpaObserveOnly = true;
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.printf("RPA_DUAL_OBSERVE %u\n",
        ble.scanner().start(scanConfig) ? 1 : 0);
    }
    else if (command == "t")
      Serial.printf("RPA_DUAL_PEER_TIMEOUT seconds=2 rc=%d\n",
        ble_hs_set_rpa_timeout(2));
    else if (command == "y")
      Serial.printf("RPA_DUAL_PEER_TIMEOUT seconds=900 rc=%d\n",
        ble_hs_set_rpa_timeout(900));
    else if (command == "R")
    {
      Serial.printf("DUAL_PEER_READY local=%s type=%u\n",
        ble.localAddress().c_str(),
        static_cast<unsigned>(ble.localAddressType()));
      Serial.printf("RPA_DUAL_PEER_INIT ble=%u classic=%u ble_error=%s:%s "
        "classic_error=%s:%s\n",
        ble.initialized() ? 1 : 0, classic.initialized() ? 1 : 0,
        ble.lastErrorName(), ble.lastErrorDetail().c_str(),
        classic.lastErrorName(), classic.lastErrorDetail().c_str());
    }
#endif
    else if (command == "u")
    {
      EspBleConnection connection;
      const bool found = bleConnectionId != 0 &&
        ble.connection(bleConnectionId, connection);
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf(
        "DUAL_BLE_LINK found=%u encrypted=%u bonded=%u security_events=%lu "
        "event=%02x status=%u enabled=%u\n",
        found ? 1 : 0, found && connection.encrypted ? 1 : 0,
        found && connection.bonded ? 1 : 0, value.security_events[0],
        value.last_security_event[0], value.last_security_status[0],
        value.last_encryption_enabled[0]);
    }
    else if (command == "o")
    {
      const uint8_t output[] = {0x02, 0xa5, 0x00, 0xff};
      Serial.printf("DUAL_PEER_OUTPUT %u\n",
        classic.hidHost().sendOutputReport(output, sizeof(output)) ? 1 : 0);
    }
    else if (command == "d")
    {
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf(
        "DUAL_PEER_DIAG tx=%lu,%lu rx=%lu,%lu ncp=%lu,%lu unknown=%lu "
        "txh=%u,%u rxh=%u,%u pb=%u,%u mode=%u modes=%lu "
        "cmd=%lu,%lu/%lu,%lu qmax=%u qfull=%lu mismatch=%lu busy=%lu "
        "masks=%lu/%lu\n",
        value.tx_acl[0], value.tx_acl[1], value.rx_acl[0], value.rx_acl[1],
        value.completed_acl[0], value.completed_acl[1], value.unknown_acl,
        value.last_tx_handle[0], value.last_tx_handle[1],
        value.last_rx_handle[0], value.last_rx_handle[1],
        value.last_tx_pb[0], value.last_tx_pb[1], value.classic_mode,
        value.classic_mode_changes,
        value.command_enqueued[0], value.command_enqueued[1],
        value.command_sent[0], value.command_sent[1],
        value.command_queue_high_water, value.command_queue_full,
        value.command_response_mismatch, value.command_unregister_busy,
        value.event_mask_commands, value.event_mask_unions);
    }
    else if (command == "v")
    {
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      for (size_t host = 0; host < ESPBLE_HCI_HOST_COUNT; ++host)
      {
        Serial.printf("DUAL_PEER_OPCODES host=%u count=%u overflow=%lu values=",
          static_cast<unsigned>(host), value.command_opcode_count[host],
          value.command_opcode_overflow[host]);
        for (size_t i = 0; i < value.command_opcode_count[host]; ++i)
          Serial.printf("%s%04x", i == 0 ? "" : ",",
            value.command_opcodes[host][i]);
        Serial.println();
      }
    }
    else if (command == "e")
    {
      ble.end();
      classic.end();
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf("DUAL_PEER_ENDED ble=%u classic=%u busy=%lu\n",
        ble.initialized() ? 1 : 0, classic.initialized() ? 1 : 0,
        value.command_unregister_busy);
    }
    else if (command == "x")
    {
      classic.end();
      Serial.printf("DUAL_PEER_REVERSE ble=%u classic=%u error=%s\n",
        ble.initialized() ? 1 : 0, classic.initialized() ? 1 : 0,
        classic.lastErrorName());
    }
    else if (command == "s")
    {
      const bool started = startDualStacks();
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf(
        "DUAL_PEER_RESTART started=%u ble=%u classic=%u busy=%lu\n",
        started ? 1 : 0, ble.initialized() ? 1 : 0,
        classic.initialized() ? 1 : 0, value.command_unregister_busy);
    }
    else if (command == "z")
    {
      const bool classicFirst = runDestructorCycle(true);
      const bool bleFirst = runDestructorCycle(false);
      const bool restarted = startDualStacks();
      Serial.printf(
        "DUAL_PEER_DESTRUCT classic_first=%u ble_first=%u restarted=%u\n",
        classicFirst ? 1 : 0, bleFirst ? 1 : 0, restarted ? 1 : 0);
    }
    else if (command == "y")
    {
      const bool started = startClassicStack();
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf(
        "DUAL_PEER_CLASSIC_REATTACH started=%u resets=%lu flow=%lu error=%s\n",
        started ? 1 : 0, value.virtual_resets,
        value.virtual_flow_control_commands, classic.lastErrorDetail().c_str());
    }
  }
  delay(1);
}
