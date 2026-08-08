#include <EspBleClassic.h>
#include <EspBle.h>
#include <EspBleHciBroker.h>

EspBleClassic classic;
EspBle ble;
bool bleConnectionRequested;
EspBleConnectionId bleConnectionId;
static const char *ServiceUuid = "c8a53600-98f4-4f2c-a231-522b5c4d9001";
static const char *CharacteristicUuid = "c8a53601-98f4-4f2c-a231-522b5c4d9001";

void printHex(const String &value)
{
  for (size_t i = 0; i < value.length(); ++i)
    Serial.printf("%02x", static_cast<uint8_t>(value[i]));
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  classic.hidHost().onConnected([](const EspBleClassicHidConnection &) {
    Serial.println("DUAL_PEER_CONNECTED");
  });
  classic.hidHost().onInputReport([](const EspBleClassicHidReport &report) {
    Serial.printf("DUAL_PEER_INPUT hex=");
    printHex(report.value);
    Serial.println();
    const uint8_t output[] = {0x02, 0xa5, 0x00, 0xff};
    Serial.printf("DUAL_PEER_OUTPUT %u\n",
      classic.hidHost().sendOutputReport(output, sizeof(output)) ? 1 : 0);
  });
  EspBleClassicConfig config;
  config.deviceName = "EspBle Dual Peer";
  if (!classic.begin(config) || !classic.hidHost().begin())
  {
    Serial.printf("DUAL_PEER_FAILED %s\n", classic.lastErrorDetail().c_str());
    return;
  }
  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle Dual Peer";
  if (!ble.begin(bleConfig))
  {
    Serial.printf("DUAL_PEER_BLE_FAILED %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (bleConnectionRequested || !result.advertisesService(ServiceUuid)) return;
    ble.scanner().stop();
    bleConnectionRequested = ble.connect(result);
    Serial.printf("DUAL_BLE_CONNECT %u\n", bleConnectionRequested ? 1 : 0);
  });
  ble.onConnected([](const EspBleConnection &connection) {
    bleConnectionId = connection.id;
    Serial.printf("DUAL_BLE_READ_REQUESTED %u\n",
      ble.readCharacteristic(connection.id, ServiceUuid, CharacteristicUuid) ? 1 : 0);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    bleConnectionId = 0;
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf("DUAL_BLE_READ success=%u value=%s classic=%u\n",
      result.success ? 1 : 0, result.value.c_str(),
      classic.hidHost().connected() ? 1 : 0);
  });
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
        "txh=%u,%u rxh=%u,%u pb=%u,%u mode=%u modes=%lu\n",
        value.tx_acl[0], value.tx_acl[1], value.rx_acl[0], value.rx_acl[1],
        value.completed_acl[0], value.completed_acl[1], value.unknown_acl,
        value.last_tx_handle[0], value.last_tx_handle[1],
        value.last_rx_handle[0], value.last_rx_handle[1],
        value.last_tx_pb[0], value.last_tx_pb[1], value.classic_mode,
        value.classic_mode_changes);
    }
  }
  delay(1);
}
