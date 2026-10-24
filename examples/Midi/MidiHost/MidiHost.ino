// en: BLE MIDI central: discover/subscribe and print MIDI, send notes
// ja: BLE MIDI Central: Discovery/購読してMIDI表示、ノート送信
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
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Install the host GATT callbacks after begin().
  // ja: begin() の後にHostのGATTコールバックを設定する。
  midi.begin();

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    deviceConnectionId = connection.id;
    // en: No security in this example, so discover immediately after connect.
    // ja: この例はsecurity無効なので、接続直後にDiscoveryする。
    midi.discover(connection.id);
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
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
      // en: Channel 0 is the first MIDI channel; the immediate Note Off below ends this note.
      // ja: channel 0は最初のMIDIチャンネルです。直後のNote Offでこの音を終了します。
      midi.sendNoteOn(deviceConnectionId, 0, 60, 100);
      midi.sendNoteOff(deviceConnectionId, 0, 60, 0);
    }
  }

  // en: Discovery/subscription/MIDI events are delivered from this update().
  // ja: Discovery/購読/MIDIイベントはこの update() から配送される。
  ble.update();
  delay(1);
}
