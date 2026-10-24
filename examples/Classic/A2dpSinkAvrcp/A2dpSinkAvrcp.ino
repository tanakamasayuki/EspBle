// en: A2DP connection, playback control, and absolute volume
// ja: A2DP接続と再生操作・absolute volume
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  bluetooth.avrcp().onPassthrough(
    [](const EspBleClassicAvrcpPassthrough &event) {
      Serial.printf("AVRCP key: 0x%02x %s\n",
        static_cast<unsigned>(event.command),
        event.state == EspBleClassicAvrcpKeyState::Pressed
          ? "pressed" : "released");
    });
  bluetooth.avrcp().onVolumeChanged(
    [](const EspBleClassicAvrcpVolume &event) {
      Serial.printf("AVRCP volume: %u%s\n", event.value,
        event.remoteCommand ? " (remote command)" : "");
    });
  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  bluetooth.a2dpSink().onConnected(
    [](const EspBleClassicA2dpConnection &connection) {
      Serial.printf("A2DP connected: %s\n", connection.peerAddress.c_str());
    });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle A2DP AVRCP Sink";
  // en: Initialize AVRCP before A2DP so the control channel is available when the remote source establishes the audio profile.
  // ja: 相手が音声プロファイルへ接続する前に制御Channelを用意するため、A2DPより先にAVRCPを初期化します。
  if (!bluetooth.begin(stackConfig) ||
      !bluetooth.avrcp().begin() ||
      !bluetooth.a2dpSink().begin())
    Serial.println(bluetooth.lastErrorDetail());
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  delay(1);
}
