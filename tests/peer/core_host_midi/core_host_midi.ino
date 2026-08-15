// DUT for the core-host BLE MIDI interoperability test. EspBle's MIDI Host
// decodes packets framed by peer_device/, which builds the MIDI service by hand
// on the BLE wrapper Arduino-ESP32 ships. Two EspBle boards would exercise one
// encoder against its own decoder; this pair does not.
#include <EspBle.h>
#include <EspBleMidiProfile.h>

EspBle ble;
EspBleMidiHost midi(ble);
bool connectRequested = false;
EspBleConnectionId connectionId = 0;
unsigned messageCount = 0;
uint8_t lastStatus = 0;
uint8_t lastData1 = 0;
uint8_t lastData2 = 0;
uint16_t lastTimestamp = 0;
unsigned sysExCount = 0;
unsigned sysExLength = 0;
uint8_t sysExFirst = 0;

void reportState()
{
  Serial.printf("MIDIHOST_STATE connected=%u ready=%u messages=%u sysex=%u\n",
    connectionId != 0 ? 1 : 0,
    connectionId != 0 && midi.ready(connectionId) ? 1 : 0, messageCount,
    sysExCount);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!ble.begin())
  {
    Serial.printf("MIDIHOST_INIT_FAILED %s:%s\n", ble.lastErrorName(),
      ble.lastErrorDetail().c_str());
    return;
  }

  midi.onMidiMessage([](const EspBleMidiMessage &message) {
    if (message.sysEx)
    {
      ++sysExCount;
      sysExLength = static_cast<unsigned>(message.sysExLength);
      sysExFirst = message.sysExLength != 0 ? message.sysExData[0] : 0;
      Serial.printf("MIDIHOST_SYSEX start=%u end=%u length=%u first=%02x\n",
        message.sysExStart ? 1 : 0, message.sysExEnd ? 1 : 0, sysExLength,
        sysExFirst);
      return;
    }
    ++messageCount;
    lastStatus = message.status;
    lastData1 = message.data1;
    lastData2 = message.data2;
    lastTimestamp = message.timestampMs;
    Serial.printf("MIDIHOST_MESSAGE index=%u status=%02x data1=%02x data2=%02x ts=%u\n",
      messageCount, message.status, message.data1, message.data2,
      message.timestampMs);
  });
  midi.begin();

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("MIDIHOST_CONNECTED id=%u\n", connection.id);
    Serial.printf("MIDIHOST_DISCOVER_REQUESTED %u\n",
      midi.discover(connection.id) ? 1 : 0);
  });

  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    Serial.printf("MIDIHOST_DISCONNECTED reason=%u\n", connection.disconnectReason);
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectRequested || !result.advertisesService(ESP_BLE_MIDI_SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectRequested = ble.connect(result);
    Serial.printf("MIDIHOST_CONNECT requested=%u peer=%s\n",
      connectRequested ? 1 : 0, result.address.c_str());
  });

  Serial.println("MIDIHOST_READY");
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "?")
    {
      Serial.println("MIDIHOST_READY");
      reportState();
    }
    else if (command == "s")
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.printf("MIDIHOST_SCAN started=%u\n",
        ble.scanner().start(scanConfig) ? 1 : 0);
    }
    else if (command == "n" && connectionId != 0)
    {
      Serial.printf("MIDIHOST_NOTE_SENT %u\n",
        midi.sendNoteOn(connectionId, 0, 0x40, 0x55) ? 1 : 0);
    }
    else if (command == "c" && connectionId != 0)
    {
      Serial.printf("MIDIHOST_CC_SENT %u\n",
        midi.sendControlChange(connectionId, 1, 0x07, 0x2a) ? 1 : 0);
    }
    else if (command == "z")
    {
      messageCount = 0;
      sysExCount = 0;
      Serial.println("MIDIHOST_COUNTERS_RESET");
    }
    else if (command == "x" && connectionId != 0)
    {
      Serial.printf("MIDIHOST_DISCONNECT_REQUESTED %u\n",
        ble.disconnect(connectionId) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
