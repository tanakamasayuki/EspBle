#include <EspBle.h>
#include <EspBleClassic.h>
#include <EspBleHciBroker.h>
#include <esp_mac.h>
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
uint8_t inputSequence;
bool advertisingConfigured;

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
    Serial.println("DUAL_BLE_SERVER_CONNECTED");
#if defined(ESPBLE_TEST_DUAL_RPA)
    Serial.printf("RPA_DUAL_SERVER_PEER addr=%s type=%u\n",
      connection.peerAddress.c_str(),
      static_cast<unsigned>(connection.peerAddressType));
#endif
  });
  ble.onDisconnected([](const EspBleConnection &) {
    bleConnected = false;
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
