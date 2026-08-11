#include <EspBleClassic.h>

EspBleClassic bluetooth;

// Replace this with the Bluetooth address of a phone or test Audio Gateway.
const char *audioGatewayAddress = "00:00:00:00:00:00";

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle HFP Client";
  if (!bluetooth.begin(stackConfig))
  {
    Serial.println(bluetooth.lastErrorDetail());
    return;
  }

  bluetooth.hfpClient().onConnectionChanged(
    [](const EspBleClassicHfpConnection &connection) {
      Serial.printf("HFP connection state: %u, peer: %s\n",
        static_cast<unsigned>(connection.state),
        connection.peerAddress.c_str());
    });
  bluetooth.hfpClient().onAudioConnectionChanged(
    [](const EspBleClassicHfpAudioConnection &audio) {
      Serial.printf("HFP audio: codec=%u frame=%u handle=%u\n",
        static_cast<unsigned>(audio.codec), audio.preferredFrameSize,
        audio.id);
    });
  bluetooth.hfpClient().onCallStateChanged(
    [](const EspBleClassicHfpCallState &call) {
      Serial.printf("HFP call: active=%u setup=%u held=%u\n",
        call.active ? 1 : 0, static_cast<unsigned>(call.setup),
        static_cast<unsigned>(call.held));
    });
  bluetooth.hfpClient().onAudio(
    [](const EspBleClassicHfpEncodedAudioView &audio) {
      // audio.data is valid only in this callback. Copy it into a bounded
      // queue for PCMFlowBluetooth; decode and device I/O belong outside it.
      // Preserve badFrame so an mSBC decoder can perform PLC.
      (void)audio;
    });

  if (!bluetooth.hfpClient().begin() ||
      !bluetooth.hfpClient().connect(audioGatewayAddress))
    Serial.println(bluetooth.lastErrorDetail());
}

void loop()
{
  bluetooth.update();
  delay(1);
}
