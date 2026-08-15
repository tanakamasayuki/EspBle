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
  bluetooth.a2dpSink().onConnected(
    [](const EspBleClassicA2dpConnection &connection) {
      Serial.printf("A2DP connected: %s\n", connection.peerAddress.c_str());
    });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle A2DP AVRCP Sink";
  // Initialize AVRCP before A2DP so the control channel is available when
  // the remote source establishes the audio profile.
  if (!bluetooth.begin(stackConfig) ||
      !bluetooth.avrcp().begin() ||
      !bluetooth.a2dpSink().begin())
    Serial.println(bluetooth.lastErrorDetail());
}

void loop()
{
  bluetooth.update();
  delay(1);
}
