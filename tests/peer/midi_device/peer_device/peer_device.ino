// midi_device peer_device: EspBle MIDI Device (Peripheral). It advertises the
// BLE MIDI service, sends Note On/Off on command, and reports MIDI received from
// the host so the DUT (a bundled-NimBLE central) can verify both directions.
#include <EspBle.h>
#include <EspBleMidiProfile.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspBle ble;
EspBleMidiDevice midi(ble);
TaskHandle_t loopTask = nullptr;

volatile uint32_t inCount = 0;
volatile uint8_t inStatus = 0;
volatile uint8_t inData1 = 0;
volatile uint8_t inData2 = 0;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  midi.onMessage([](const EspBleMidiMessage &message) {
    if (message.sysEx)
      return;
    inStatus = message.status;
    inData1 = message.data1;
    inData2 = message.data2;
    ++inCount;
  });
  if (!midi.begin())
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle MIDI Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle MIDI Peer");
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'r')
    {
      Serial.printf("READY %u\n", midi.ready() ? 1 : 0);
    }
    else if (command == '1')
    {
      Serial.printf("NOTE_ON_SENT %u\n", midi.noteOn(0, 60, 100) ? 1 : 0);
    }
    else if (command == '0')
    {
      Serial.printf("NOTE_OFF_SENT %u\n", midi.noteOff(0, 60, 0) ? 1 : 0);
    }
    else if (command == 'y')
    {
      // A 300-byte SysEx payload always exceeds one packet (packets are capped
      // at 244 bytes), forcing the multi-packet send queue regardless of MTU.
      static uint8_t sysex[302];
      sysex[0] = 0xF0;
      for (size_t i = 0; i < 300; ++i)
        sysex[1 + i] = static_cast<uint8_t>(i & 0x7F);
      sysex[301] = 0xF7;
      Serial.printf("SYSEX_SENT %u\n", midi.sendSysEx(sysex, sizeof(sysex)) ? 1 : 0);
    }
    else if (command == 'q')
    {
      Serial.printf("DEVICE_IN count=%u status=%u data1=%u data2=%u context=%s\n",
        static_cast<unsigned>(inCount), inStatus, inData1, inData2, contextName());
    }
  }
  ble.update();
  delay(1);
}
