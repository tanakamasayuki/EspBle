// en: MidiHost - connect to a BLE MIDI peripheral as a central: scan the MIDI
//     service -> connect -> discover -> subscribe -> print MIDI messages. Send a
//     note from serial input. Pairs with the MidiDevice example or a commercial
//     BLE MIDI instrument.
// ja: MidiHost - CentralとしてBLE MIDI Peripheralへ接続する。MIDI Serviceをscan →
//     接続 → Discovery → 購読 → MIDIメッセージ表示。Serial入力でノート送信。
//     MidiDevice example や市販BLE MIDI楽器と接続できる。
#include <EspBle.h>
#include <EspBleMidiProfile.h>

EspBle ble;
EspBleMidiHost midi(ble);
EspBleConnectionId deviceConnectionId = 0;

void setup()
{
  Serial.begin(115200);

  // en: Decoded MIDI arrives here (mirrors EspUsbHost's onMidiMessage).
  // ja: デコード済みMIDIがここへ届く（EspUsbHostのonMidiMessageに対応）。
  midi.onMidiMessage([](const EspBleMidiMessage &message) {
    if (message.sysEx)
    {
      Serial.printf("SysEx chunk: start=%u end=%u length=%u\n",
        message.sysExStart ? 1 : 0, message.sysExEnd ? 1 : 0,
        static_cast<unsigned>(message.sysExLength));
      return;
    }
    Serial.printf("MIDI: conn=%u status=0x%02x data1=%u data2=%u ts=%u\n",
      static_cast<unsigned>(message.connectionId),
      message.status, message.data1, message.data2, message.timestampMs);
  });

  EspBleConfig config;
  config.deviceName = "EspBle MIDI Host";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Install the host GATT callbacks after begin().
  // ja: begin() の後にHostのGATTコールバックを設定する。
  midi.begin();

  ble.onConnected([](const EspBleConnection &connection) {
    deviceConnectionId = connection.id;
    // en: No security in this example, so discover immediately after connect.
    // ja: この例はsecurity無効なので、接続直後にDiscoveryする。
    midi.discover(connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    deviceConnectionId = 0;
  });

  // en: Connect to a connectable device advertising the BLE MIDI service.
  // ja: BLE MIDI Serviceをadvertiseするconnectableな機器へ接続する。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(ESP_BLE_MIDI_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  if (Serial.available() > 0 && deviceConnectionId != 0 && midi.ready(deviceConnectionId))
  {
    const char command = Serial.read();
    if (command == 'n')
    {
      midi.sendNoteOn(deviceConnectionId, 0, 60, 100);
      midi.sendNoteOff(deviceConnectionId, 0, 60, 0);
    }
  }

  // en: Discovery/subscription/MIDI events are delivered from this update().
  // ja: Discovery/購読/MIDIイベントはこの update() から配送される。
  ble.update();
  delay(1);
}
