#include <EspBle.h>
#include <EspBleClassic.h>
#include <EspBleHciBroker.h>
#include <esp_mac.h>
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL) && defined(CONFIG_IDF_TARGET_ESP32)
#include <nimble_esp32/include/host/ble_gap.h>
#include <esp_gap_bt_api.h>
extern "C" esp_err_t espble_bd_esp_bt_gap_set_scan_mode(
  esp_bt_connection_mode_t connectionMode,
  esp_bt_discovery_mode_t discoveryMode);
#endif
#if defined(ESPBLE_TEST_DUAL_RPA)
#include <nimble_esp32/include/host/ble_hs_pvcy.h>
#endif

static const char *ServiceUuid = "c8a53600-98f4-4f2c-a231-522b5c4d9001";
static const char *CharacteristicUuid = "c8a53601-98f4-4f2c-a231-522b5c4d9001";
static const uint8_t ReportDescriptor[] = {
  0x06, 0x00, 0xff, 0x09, 0x01, 0xa1, 0x01,
  0x85, 0x01, 0x15, 0x00, 0x26, 0xff, 0x00,
  0x75, 0x08, 0x95, 0x04, 0x09, 0x01, 0x81, 0x02,
  0x85, 0x02, 0x95, 0x03, 0x09, 0x02, 0x91, 0x02, 0xc0,
};

EspBleClassic classic;
EspBle ble;
EspBleGattService service;
EspBleGattCharacteristic characteristic;
bool inputSent;
bool bleConnected;
EspBleConnectionId bleConnectionId;
uint8_t inputSequence;
bool advertisingConfigured;

#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL) && defined(CONFIG_IDF_TARGET_ESP32)
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
    // The Classic API posts work to Bluedroid's BTC task. Wait until the
    // broker-visible command count settles.
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
    // This command enters after every command accepted above. Its synchronous
    // completion proves the broker FIFO drained all earlier transactions.
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
  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "EspBle Dual Host";
  EspBleClassicHidDeviceConfig hidConfig;
  hidConfig.name = "EspBle Dual HID";
  hidConfig.description = "EspBle dual-host smoke";
  hidConfig.provider = "EspBle";
  hidConfig.reportDescriptor = ReportDescriptor;
  hidConfig.reportDescriptorLength = sizeof(ReportDescriptor);
  return classic.begin(classicConfig) && classic.hidDevice().begin(hidConfig);
}

bool startDualStacks()
{
  if (!startClassicStack()) return false;

  if (!service.valid())
  {
    EspBleGattCharacteristicConfig characteristicConfig;
    characteristicConfig.readable = true;
    characteristicConfig.encryptedRead = true;
    service = ble.gattServer().addService(ServiceUuid);
    characteristic = ble.gattServer().addCharacteristic(
      service, CharacteristicUuid, characteristicConfig);
  }
  ble.gattServer().setValue(characteristic, String("dual-ready"));
  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle Dual Host";
  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;
  bleConfig.security.pairOnConnect = true;
#if defined(ESPBLE_TEST_DUAL_RPA)
  bleConfig.ownAddressType = EspBleOwnAddressType::ResolvablePrivate;
#endif
  if (!service.valid() || !characteristic.valid() || !ble.begin(bleConfig))
    return false;
  if (!advertisingConfigured)
  {
    ble.advertising().setName("EspBle Dual Host");
    ble.advertising().addServiceUuid(ServiceUuid);
    advertisingConfigured = true;
  }
  return ble.advertising().start();
}

bool runDestructorCycle(bool classicFirst)
{
  EspBleClassic *temporaryClassic = new EspBleClassic();
  EspBle *temporaryBle = new EspBle();
  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "EspBle Destructor";
  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle Destructor";
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

  classic.hidDevice().onConnected([](const EspBleClassicHidConnection &) {
    Serial.println("DUAL_CLASSIC_CONNECTED");
  });
  classic.hidDevice().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("DUAL_CLASSIC_DISCONNECTED");
  });
  classic.hidDevice().onOutputReport([](const EspBleClassicHidReport &report) {
    Serial.printf("DUAL_CLASSIC_OUTPUT id=%u hex=", report.reportId);
    printHex(report.value);
    Serial.println();
  });
  ble.onConnected([](const EspBleConnection &connection) {
    bleConnected = true;
    bleConnectionId = connection.id;
    Serial.println("DUAL_BLE_SERVER_CONNECTED");
#if defined(ESPBLE_TEST_DUAL_RPA)
    Serial.printf("RPA_DUAL_SERVER_PEER addr=%s type=%u\n",
      connection.peerAddress.c_str(),
      static_cast<unsigned>(connection.peerAddressType));
#endif
  });
  ble.onDisconnected([](const EspBleConnection &) {
    bleConnected = false;
    bleConnectionId = 0;
    Serial.println("DUAL_BLE_SERVER_DISCONNECTED");
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "DUAL_BLE_SERVER_SECURITY success=%u encrypted=%u bonded=%u key=%u classic=%u\n",
      event.success ? 1 : 0, event.connection.encrypted ? 1 : 0,
      event.connection.bonded ? 1 : 0, event.connection.encryptionKeySize,
      classic.hidDevice().connected() ? 1 : 0);
  });

  if (!startDualStacks())
  {
    Serial.printf("DUAL_START_FAILED classic=%s ble=%s\n",
      classic.lastErrorDetail().c_str(), ble.lastErrorDetail().c_str());
    return;
  }
#if !defined(ESPBLE_TEST_DUAL_RPA)
  (void)ble.deleteAllBonds();
#endif

  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf(
    "DUAL_READY classic=%02x:%02x:%02x:%02x:%02x:%02x ble=%s type=%u\n",
    address[0], address[1], address[2], address[3], address[4], address[5],
    ble.localAddress().c_str(), static_cast<unsigned>(ble.localAddressType()));
}

void loop()
{
  classic.update();
  ble.update();
  if (classic.hidDevice().connected() && !inputSent)
  {
    const uint8_t report[] = {0x00, 0x7f, 0x80, inputSequence++};
    inputSent = classic.hidDevice().sendInputReport(1, report, sizeof(report));
    Serial.printf("DUAL_CLASSIC_INPUT %u\n", inputSent ? 1 : 0);
  }
  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'i') inputSent = false;
    else if (command == 'a')
      Serial.printf("DUAL_BLE_ADVERTISING %u\n",
        ble.advertising().start() ? 1 : 0);
    else if (command == 'n')
      Serial.printf("DUAL_BLE_BONDS %u\n",
        static_cast<unsigned>(ble.bondCount()));
    else if (command == 'h')
      printHeap("DUAL_HEAP");
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL) && defined(CONFIG_IDF_TARGET_ESP32)
    else if (command == 'j')
      runCommandContention("DUAL");
#if defined(ESPBLE_HCI_BACKPRESSURE_TEST)
    else if (command == 'K')
      runBackpressure("DUAL");
#endif
#endif
#if defined(ESPBLE_TEST_DUAL_RPA)
    else if (command == 'X')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("RPA_DUAL_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0,
        static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 'Z')
    {
      Serial.println("RPA_DUAL_RESTARTING");
      Serial.flush();
      ESP.restart();
    }
    else if (command == 'T')
      Serial.printf("RPA_DUAL_TIMEOUT seconds=2 rc=%d\n",
        ble_hs_set_rpa_timeout(2));
    else if (command == 'Y')
      Serial.printf("RPA_DUAL_TIMEOUT seconds=900 rc=%d\n",
        ble_hs_set_rpa_timeout(900));
    else if (command == 'S')
      Serial.printf("RPA_DUAL_ADVERTISING_STOP %u\n",
        ble.advertising().stop() ? 1 : 0);
    else if (command == 'F')
      Serial.printf("RPA_DUAL_FINITE_ADVERTISING seconds=8 success=%u\n",
        ble.advertising().start(8) ? 1 : 0);
    else if (command == 'R')
    {
      uint8_t address[6] = {};
      esp_read_mac(address, ESP_MAC_BT);
      Serial.printf(
        "DUAL_READY classic=%02x:%02x:%02x:%02x:%02x:%02x ble=%s type=%u\n",
        address[0], address[1], address[2], address[3], address[4], address[5],
        ble.localAddress().c_str(),
        static_cast<unsigned>(ble.localAddressType()));
      Serial.printf("RPA_DUAL_INIT ble=%u classic=%u ble_error=%s:%s "
        "classic_error=%s:%s\n",
        ble.initialized() ? 1 : 0, classic.initialized() ? 1 : 0,
        ble.lastErrorName(), ble.lastErrorDetail().c_str(),
        classic.lastErrorName(), classic.lastErrorDetail().c_str());
    }
#endif
    else if (command == '?')
      Serial.printf("DUAL_STATE adv=%u classic=%u ble=%u\n",
        ble.advertising().isAdvertising() ? 1 : 0,
        classic.hidDevice().connected() ? 1 : 0,
        bleConnected ? 1 : 0);
    else if (command == 'd')
    {
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf(
        "DUAL_DIAG tx=%lu,%lu rx=%lu,%lu ncp=%lu,%lu unknown=%lu "
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
    else if (command == 'v')
    {
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      for (size_t host = 0; host < ESPBLE_HCI_HOST_COUNT; ++host)
      {
        Serial.printf("DUAL_OPCODES host=%u count=%u overflow=%lu values=",
          static_cast<unsigned>(host), value.command_opcode_count[host],
          value.command_opcode_overflow[host]);
        for (size_t i = 0; i < value.command_opcode_count[host]; ++i)
          Serial.printf("%s%04x", i == 0 ? "" : ",",
            value.command_opcodes[host][i]);
        Serial.println();
      }
    }
    else if (command == 'e')
    {
      ble.end();
      classic.end();
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf("DUAL_ENDED ble=%u classic=%u busy=%lu\n",
        ble.initialized() ? 1 : 0, classic.initialized() ? 1 : 0,
        value.command_unregister_busy);
    }
    else if (command == 'x')
    {
      classic.end();
      Serial.printf("DUAL_REVERSE ble=%u classic=%u error=%s\n",
        ble.initialized() ? 1 : 0, classic.initialized() ? 1 : 0,
        classic.lastErrorName());
    }
    else if (command == 's')
    {
      const bool started = startDualStacks();
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf("DUAL_RESTART started=%u ble=%u classic=%u busy=%lu\n",
        started ? 1 : 0, ble.initialized() ? 1 : 0,
        classic.initialized() ? 1 : 0, value.command_unregister_busy);
    }
    else if (command == 'z')
    {
      const bool classicFirst = runDestructorCycle(true);
      const bool bleFirst = runDestructorCycle(false);
      const bool restarted = startDualStacks();
      Serial.printf(
        "DUAL_DESTRUCT classic_first=%u ble_first=%u restarted=%u\n",
        classicFirst ? 1 : 0, bleFirst ? 1 : 0, restarted ? 1 : 0);
    }
    else if (command == 'y')
    {
      const bool started = startClassicStack();
      espble_hci_broker_diagnostics_t value = {};
      espble_hci_broker_get_diagnostics(&value);
      Serial.printf(
        "DUAL_CLASSIC_REATTACH started=%u resets=%lu flow=%lu error=%s\n",
        started ? 1 : 0, value.virtual_resets,
        value.virtual_flow_control_commands, classic.lastErrorDetail().c_str());
    }
  }
  delay(1);
}
