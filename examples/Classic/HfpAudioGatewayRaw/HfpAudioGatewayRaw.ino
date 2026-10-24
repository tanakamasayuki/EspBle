// en: Small telephony model and raw CVSD/mSBC SCO transport
// ja: 小さいtelephony modelとraw CVSD/mSBC SCO transport
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "EspBle HFP Audio Gateway";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
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
    // en: Tell the accessory who makes the ring sound. This gateway sends no ring tone of its own, so the headset has to beep — an accessory told the opposite waits for audio that never arrives.
    // ja: 着信音をどちらが鳴らすか相手へ知らせます。このGatewayは着信音声を送らないため、ヘッドセット自身に鳴らしてもらいます。
    if (gateway.serviceLevelConnected())
      (void)gateway.setInBandRingTone(false);
  });
  gateway.onCommand([&gateway](
    const EspBleClassicHfpAudioGatewayCommand &command) {
    if (command.type == EspBleClassicHfpAudioGatewayCommandType::Dial)
    {
      // en: A real application asks its telephony provider before accepting.
      // ja: 実際のアプリでは、受け付ける前に通話サービス側へ問い合わせます。
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
  // en: This encoded SCO audio callback runs in the Bluetooth host context. Copy data before return, keep work bounded and do not replace the callback or call end from inside it.
  // ja: この符号化SCO音声コールバックはBluetooth Hostの実行コンテキストで動きます。戻る前にデータをコピーし、処理を短く保ち、内部からコールバックの再登録やendを呼ばないようにします。
  gateway.onAudio([](const EspBleClassicHfpEncodedAudioView &audio) {
    // en: Copy the encoded CVSD/mSBC payload here before returning. Decoding, buffering, PCM processing, and device I/O belong outside EspBle.
    // ja: 符号化されたCVSD／mSBCデータを戻る前にコピーします。デコード・バッファ・PCM処理・デバイス入出力はEspBleの外で行います。
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
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  delay(1);
}
