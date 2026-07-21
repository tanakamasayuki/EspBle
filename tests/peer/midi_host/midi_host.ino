// midi_host DUT: EspBle MIDI Host (Central). It connects to the BLE MIDI
// peripheral, discovers and subscribes to the I/O characteristic, and captures
// decoded MIDI so the test can assert running-status decoding. The peer_device
// is a bundled-NimBLE peripheral, so decoding is validated against an
// independent sender.
#include <EspBle.h>
#include <EspBleMidiProfile.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspBle ble;
EspBleMidiHost midi(ble);
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

volatile uint32_t midiCount = 0;
uint8_t capStatus[8] = {0};
uint8_t capData1[8] = {0};
uint8_t capData2[8] = {0};
uint16_t capTs[8] = {0};

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  if (!ble.begin())
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  midi.onMidiMessage([](const EspBleMidiMessage &message) {
    if (message.sysEx)
      return;
    const uint32_t index = midiCount % 8;
    capStatus[index] = message.status;
    capData1[index] = message.data1;
    capData2[index] = message.data2;
    capTs[index] = message.timestampMs;
    ++midiCount;
  });
  midi.begin();

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("HOST_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    midi.discover(connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("HOST_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(ESP_BLE_MIDI_SERVICE_UUID))
      return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
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
      Serial.println(ble.scanner().start(scan) ? "HOST_SCAN_STARTED" : "HOST_SCAN_START_FAILED");
    }
    else if (command == 'r')
    {
      Serial.printf("HOST_READY %u\n",
        (connectionId != 0 && midi.ready(connectionId)) ? 1 : 0);
    }
    else if (command == 'c')
    {
      midiCount = 0;
      Serial.println("HOST_MIDI_RESET");
    }
    else if (command == 'n' && connectionId != 0)
    {
      Serial.printf("HOST_NOTE_SENT %u\n", midi.sendNoteOn(connectionId, 0, 64, 100) ? 1 : 0);
    }
    else if (command == 'y' && connectionId != 0)
    {
      static uint8_t sysex[302];
      sysex[0] = 0xF0;
      for (size_t i = 0; i < 300; ++i)
        sysex[1 + i] = static_cast<uint8_t>(i & 0x7F);
      sysex[301] = 0xF7;
      Serial.printf("HOST_SYSEX_SENT %u\n", midi.sendSysEx(connectionId, sysex, sizeof(sysex)) ? 1 : 0);
    }
    else if (command == 'q')
    {
      Serial.printf(
        "HOST_MIDI count=%u m0_status=%u m0_d1=%u m0_d2=%u m0_ts=%u "
        "m1_status=%u m1_d1=%u m1_d2=%u m1_ts=%u context=%s\n",
        static_cast<unsigned>(midiCount),
        capStatus[0], capData1[0], capData2[0], capTs[0],
        capStatus[1], capData1[1], capData2[1], capTs[1],
        contextName());
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "HOST_DISCONNECT_REQUESTED" : "HOST_DISCONNECT_FAILED");
    }
  }
  ble.update();
  delay(1);
}
