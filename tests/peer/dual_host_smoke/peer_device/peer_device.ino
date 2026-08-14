#include <EspBleClassic.h>
#include <EspBle.h>
#include <EspBleHciBroker.h>
#include <esp_mac.h>
#include <esp_system.h>
#if defined(CONFIG_IDF_TARGET_ESP32)
#include <nimble_esp32/include/host/ble_gap.h>
#include <esp_gap_bt_api.h>
extern "C" esp_err_t espble_bd_esp_bt_gap_set_scan_mode(
  esp_bt_connection_mode_t connectionMode,
  esp_bt_discovery_mode_t discoveryMode);
#endif
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

#if defined(CONFIG_IDF_TARGET_ESP32)
struct CommandContentionContext
{
  volatile uint32_t accepted = 0;
  volatile bool done = false;
};

void classicCommandContentionTask(void *argument)
{
  CommandContentionContext *context =
    static_cast<CommandContentionContext *>(argument);
  for (uint32_t index = 0; index < 20; ++index)
  {
    const esp_bt_discovery_mode_t discoveryMode = (index & 1) != 0
      ? ESP_BT_GENERAL_DISCOVERABLE
      : ESP_BT_NON_DISCOVERABLE;
    if (espble_bd_esp_bt_gap_set_scan_mode(
          ESP_BT_CONNECTABLE, discoveryMode) == ESP_OK)
      ++context->accepted;
    vTaskDelay(1);
  }
  context->done = true;
  vTaskDelete(nullptr);
}

void runCommandContention(const char *prefix)
{
  EspBleConnection connection;
  if (bleConnectionId == 0 || !ble.connection(bleConnectionId, connection))
  {
    Serial.printf("%s_CONTENTION connection=0\n", prefix);
    return;
  }
  espble_hci_broker_diagnostics_t before = {};
  espble_hci_broker_get_diagnostics(&before);
  CommandContentionContext context;
  const BaseType_t taskStarted = xTaskCreate(
    classicCommandContentionTask, "classic_cmd_stress", 3072, &context, 2, nullptr);
  uint32_t rssiSuccess = 0;
  int8_t rssi = 0;
  if (taskStarted == pdPASS)
  {
    for (uint32_t index = 0; index < 20; ++index)
    {
      if (ble_gap_conn_rssi(connection.handle, &rssi) == 0) ++rssiSuccess;
      vTaskDelay(1);
    }
    // The task owns a pointer to this stack context, so do not return until it
    // has stopped using it. The pytest serial timeout still catches a hung DUT.
    while (!context.done)
      vTaskDelay(1);
    uint32_t previousClassicCommands = UINT32_MAX;
    uint32_t stableSince = millis();
    const uint32_t settleDeadline = millis() + 5000;
    while (static_cast<int32_t>(settleDeadline - millis()) > 0)
    {
      espble_hci_broker_diagnostics_t current = {};
      espble_hci_broker_get_diagnostics(&current);
      const uint32_t classicCommands =
        current.command_enqueued[1] - before.command_enqueued[1];
      if (classicCommands != previousClassicCommands)
      {
        previousClassicCommands = classicCommands;
        stableSince = millis();
      }
      if (millis() - stableSince >= 250) break;
      vTaskDelay(1);
    }
    if (context.done && ble_gap_conn_rssi(connection.handle, &rssi) == 0)
      ++rssiSuccess;
  }
  espble_hci_broker_diagnostics_t after = {};
  espble_hci_broker_get_diagnostics(&after);
  Serial.printf(
    "%s_CONTENTION task=%u classic=%lu rssi=%lu tx=%lu,%lu/%lu,%lu "
    "qmax=%u qfull=%lu mismatch=%lu busy=%lu unknown=%lu last_rssi=%d\n",
    prefix, taskStarted == pdPASS ? 1 : 0,
    static_cast<unsigned long>(context.accepted),
    static_cast<unsigned long>(rssiSuccess),
    static_cast<unsigned long>(
      after.command_enqueued[0] - before.command_enqueued[0]),
    static_cast<unsigned long>(
      after.command_enqueued[1] - before.command_enqueued[1]),
    static_cast<unsigned long>(
      after.command_sent[0] - before.command_sent[0]),
    static_cast<unsigned long>(
      after.command_sent[1] - before.command_sent[1]),
    after.command_queue_high_water,
    static_cast<unsigned long>(after.command_queue_full),
    static_cast<unsigned long>(after.command_response_mismatch),
    static_cast<unsigned long>(after.command_unregister_busy),
    static_cast<unsigned long>(after.unknown_acl), rssi);
}

#if defined(ESPBLE_HCI_BACKPRESSURE_TEST)
struct BackpressureContext
{
  espble_hci_host_t host;
  volatile bool *start;
  volatile bool done = false;
  uint32_t accepted = 0;
  uint32_t full = 0;
  uint32_t other = 0;
};

void backpressureTask(void *argument)
{
  BackpressureContext *context = static_cast<BackpressureContext *>(argument);
  static const uint8_t ReadBdAddr[] = {0x01, 0x09, 0x10, 0x00};
  while (!*context->start) vTaskDelay(1);
  for (uint32_t index = 0; index < 12; ++index)
  {
    const esp_err_t result = espble_hci_broker_send(
      context->host, ReadBdAddr, sizeof(ReadBdAddr));
    if (result == ESP_OK) ++context->accepted;
    else if (result == ESP_ERR_NO_MEM) ++context->full;
    else ++context->other;
  }
  context->done = true;
  vTaskDelete(nullptr);
}

void runBackpressure(const char *prefix)
{
  if (espble_hci_broker_test_begin_backpressure() != ESP_OK)
  {
    Serial.printf("%s_BACKPRESSURE begin=0\n", prefix);
    return;
  }
  volatile bool start = false;
  BackpressureContext contexts[2] = {
    {ESPBLE_HCI_HOST_NIMBLE, &start},
    {ESPBLE_HCI_HOST_CLASSIC, &start},
  };
  uint32_t tasks = 0;
  for (size_t index = 0; index < 2; ++index)
  {
    if (xTaskCreate(backpressureTask, "hci_queue_fill", 4096,
          &contexts[index], 2, nullptr) == pdPASS)
      ++tasks;
    else
      contexts[index].done = true;
  }
  start = true;
  while (!contexts[0].done || !contexts[1].done) vTaskDelay(1);

  uint16_t queued = 0;
  uint16_t highWater = 0;
  uint32_t brokerFull = 0;
  const bool restored = espble_hci_broker_test_end_backpressure(
    &queued, &highWater, &brokerFull) == ESP_OK;
  Serial.printf(
    "%s_BACKPRESSURE tasks=%lu accepted=%lu full=%lu other=%lu "
    "queued=%u qmax=%u qfull=%lu restored=%u\n",
    prefix, static_cast<unsigned long>(tasks),
    static_cast<unsigned long>(contexts[0].accepted + contexts[1].accepted),
    static_cast<unsigned long>(contexts[0].full + contexts[1].full),
    static_cast<unsigned long>(contexts[0].other + contexts[1].other),
    queued, highWater, static_cast<unsigned long>(brokerFull),
    restored ? 1 : 0);
}
#endif
#endif

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

bool startBleStack()
{
  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle Dual Peer";
  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;
  bleConfig.security.pairOnConnect = true;
#if defined(ESPBLE_TEST_DUAL_RPA)
  bleConfig.ownAddressType = EspBleOwnAddressType::ResolvablePrivate;
#else
  bleConfig.security.mitm = true;
  bleConfig.security.ioCapability = EspBleSecurityIoCapability::KeyboardOnly;
#endif
  return ble.begin(bleConfig);
}

bool startDualStacks()
{
  return startClassicStack() && startBleStack();
}

// Walks every host combination the broker can be in, in both start orders, so a
// sketch can move between BLE-only, Classic-only and both without rebooting.
bool runHostModeCycle()
{
  ble.end();
  classic.end();

  if (!startBleStack()) return false;
  if (!ble.initialized() || classic.initialized()) return false;
  if (!startClassicStack()) return false;
  if (!ble.initialized() || !classic.initialized()) return false;

  ble.end();
  if (ble.initialized() || !classic.initialized()) return false;
  classic.end();
  if (classic.initialized()) return false;

  if (!startClassicStack()) return false;
  if (ble.initialized() || !classic.initialized()) return false;
  if (!startBleStack()) return false;
  if (!ble.initialized() || !classic.initialized()) return false;

  classic.end();
  if (!ble.initialized() || classic.initialized()) return false;
  ble.end();
  return !ble.initialized() && !classic.initialized();
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
  classic.hidHost().onConnectionFailed(
    [](const EspBleClassicHidConnectionFailure &failure) {
      Serial.printf(
        "DUAL_PEER_CONNECT_FAILED address=%s error=%u detail=%s connected=%u\n",
        failure.peerAddress.c_str(), static_cast<unsigned>(failure.error),
        failure.detail.c_str(), classic.hidHost().connected() ? 1 : 0);
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
  // Keep the saved LTK when the peer-disappearance test deliberately performs
  // a software reset. A normal power-on/upload still starts from a clean bond
  // store, preserving the original test isolation.
  if (esp_reset_reason() != ESP_RST_SW)
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
#if !defined(ESPBLE_TEST_DUAL_RPA)
    else if (command == "f")
    {
      uint8_t address[6] = {};
      esp_read_mac(address, ESP_MAC_BT);
      char selfAddress[18];
      snprintf(selfAddress, sizeof(selfAddress),
        "%02x:%02x:%02x:%02x:%02x:%02x",
        address[0], address[1], address[2],
        address[3], address[4], address[5]);
      Serial.printf("DUAL_PEER_FAIL_CONNECT address=%s requested=%u\n",
        selfAddress,
        classic.hidHost().connect(selfAddress) ? 1 : 0);
    }
#endif
    else if (command == "l")
      Serial.printf("DUAL_PEER_CLASSIC_DISCONNECT %u\n",
        classic.hidHost().disconnect() ? 1 : 0);
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
    else if (command.startsWith("p") && command.length() > 1)
    {
      const uint32_t passkey =
        static_cast<uint32_t>(strtoul(command.c_str() + 1, nullptr, 10));
      Serial.printf("DUAL_PEER_PASSKEY accepted=%u value=%06u\n",
        ble.providePasskey(passkey) ? 1 : 0,
        static_cast<unsigned>(passkey));
    }
    else if (command == "h")
      printHeap("DUAL_PEER_HEAP");
    else if (command == "L")
    {
      static uint8_t oversized[
        EspBleClassicHidHost::MaximumReportLength + 1] = {};
      const bool nullRejected = !classic.hidHost().sendOutputReport(nullptr, 1);
      const bool oversizedRejected = !classic.hidHost().sendOutputReport(
        oversized, sizeof(oversized));
      Serial.printf(
        "DUAL_PEER_INVALID_REPORT null=%u oversized=%u error=%s connected=%u\n",
        nullRejected ? 1 : 0, oversizedRejected ? 1 : 0,
        classic.lastErrorName(), classic.hidHost().connected() ? 1 : 0);
    }
#if !defined(ESPBLE_TEST_DUAL_RPA)
    else if (command == "P")
    {
      Serial.println("DUAL_PEER_RESTARTING");
      Serial.flush();
      ESP.restart();
    }
#endif
#if defined(CONFIG_IDF_TARGET_ESP32)
    else if (command == "j")
      runCommandContention("DUAL_PEER");
#if defined(ESPBLE_HCI_BACKPRESSURE_TEST)
    else if (command == "K")
      runBackpressure("DUAL_PEER");
#endif
#endif
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
    else if (command == "f")
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      scanConfig.durationSeconds = 8;
      Serial.printf("RPA_DUAL_FINITE_SCAN seconds=8 success=%u\n",
        ble.scanner().start(scanConfig) ? 1 : 0);
    }
    else if (command == "w")
      Serial.printf("RPA_DUAL_SCAN_STATE active=%u\n",
        ble.scanner().isScanning() ? 1 : 0);
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
    else if (command == "M")
    {
      const uint32_t before = ESP.getFreeHeap();
      unsigned completed = 0;
      for (unsigned cycle = 0; cycle < 5; ++cycle)
      {
        if (!runHostModeCycle()) break;
        ++completed;
      }
      // Sample the heap while both hosts are still stopped, so it compares
      // against the pre-cycle sample taken in the same state.
      const uint32_t after = ESP.getFreeHeap();
      const bool restarted = startDualStacks();
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf(
        "DUAL_PEER_MODES completed=%u restarted=%u busy=%lu qfull=%lu "
        "mismatch=%lu unknown=%lu heap_before=%lu heap_after=%lu error=%s:%s\n",
        completed, restarted ? 1 : 0, value.command_unregister_busy,
        value.command_queue_full, value.command_response_mismatch,
        value.unknown_acl, static_cast<unsigned long>(before),
        static_cast<unsigned long>(after),
        classic.lastErrorName(), ble.lastErrorName());
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
