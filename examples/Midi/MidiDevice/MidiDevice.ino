// en: MidiDevice - advertise a BLE MIDI peripheral. Send Note On/Off from serial
//     input and print any MIDI a connected host sends back. Pairs with the
//     MidiHost example or any BLE MIDI host (e.g. a phone/tablet DAW).
// ja: MidiDevice - BLE MIDI PeripheralをAdvertiseする。Serial入力でNote On/Offを送信し、
//     接続したHostから届くMIDIを表示する。MidiHost example や一般的なBLE MIDI Host
//     （スマホ/タブレットのDAW等）と接続できる。
#include <EspBle.h>
#include <EspBleMidiProfile.h>

EspBle ble;
EspBleMidiDevice midi(ble);

void setup()
{
  Serial.begin(115200);

  // en: Print MIDI received from the host (host -> device).
  // ja: Hostから届いたMIDIを表示する（Host -> Device）。
  midi.onMessage([](const EspBleMidiMessage &message) {
    if (message.sysEx)
    {
      Serial.printf("SysEx chunk: start=%u end=%u length=%u\n",
        message.sysExStart ? 1 : 0, message.sysExEnd ? 1 : 0,
        static_cast<unsigned>(message.sysExLength));
      return;
    }
    Serial.printf("MIDI in: status=0x%02x data1=%u data2=%u ts=%u\n",
      message.status, message.data1, message.data2, message.timestampMs);
  });

  // en: Register the MIDI service and advertised UUID BEFORE begin().
  // ja: MIDI Serviceと広告UUIDを begin() より前に登録する。
  midi.begin();

  EspBleConfig config;
  config.deviceName = "EspBle MIDI";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'n' && midi.ready())
    {
      // en: middle C Note On (channel 0), then Note Off.
      // ja: 中央ハ（Note On, channel 0）→ Note Off。
      midi.noteOn(0, 60, 100);
      midi.noteOff(0, 60, 0);
    }
  }

  // en: onMessage callbacks are delivered from this update().
  // ja: onMessageコールバックはこの update() から配送される。
  ble.update();
  delay(1);
}
