#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "EspBle HFP Audio Gateway";
  if (!bluetooth.begin(classicConfig))
  {
    Serial.println(bluetooth.lastErrorDetail());
    return;
  }

  auto &gateway = bluetooth.hfpAudioGateway();
  gateway.onConnectionChanged([&gateway](
    const EspBleClassicHfpConnection &event) {
    Serial.printf("SLC state=%u peer=%s\n",
      static_cast<unsigned>(event.state), event.peerAddress.c_str());
    // Tell the accessory who makes the ring sound. This gateway sends no ring
    // tone of its own, so the headset has to beep — an accessory told the
    // opposite waits for audio that never arrives.
    if (gateway.serviceLevelConnected())
      (void)gateway.setInBandRingTone(false);
  });
  gateway.onCommand([&gateway](
    const EspBleClassicHfpAudioGatewayCommand &command) {
    if (command.type == EspBleClassicHfpAudioGatewayCommandType::Dial)
    {
      // A real application asks its telephony provider before accepting.
      if (gateway.respondToCommand(true) &&
          gateway.reportOutgoingCall(command.value.c_str()))
        (void)gateway.reportCallActive();
    }
    else if (command.type == EspBleClassicHfpAudioGatewayCommandType::Answer)
    {
      (void)gateway.reportCallActive();
    }
    else if (command.type == EspBleClassicHfpAudioGatewayCommandType::Hangup)
    {
      (void)gateway.reportCallEnded();
    }
  });
  gateway.onAudio([](const EspBleClassicHfpEncodedAudioView &audio) {
    // Copy the encoded CVSD/mSBC payload here before returning. Decoding,
    // buffering, PCM processing, and device I/O belong outside EspBle.
    Serial.printf("SCO codec=%u bytes=%u bad=%u\n",
      static_cast<unsigned>(audio.codec), static_cast<unsigned>(audio.length),
      audio.badFrame ? 1 : 0);
  });

  EspBleClassicHfpAudioGatewayConfig gatewayConfig;
  gatewayConfig.operatorName = "EspBle";
  gatewayConfig.subscriberNumber = "5550000";
  if (!gateway.begin(gatewayConfig))
    Serial.println(bluetooth.lastErrorDetail());
}

void loop()
{
  bluetooth.update();
  delay(1);
}
