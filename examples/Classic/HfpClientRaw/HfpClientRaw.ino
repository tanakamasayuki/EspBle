// en: The headset side of HFP: a call's control path plus already-encoded SCO
//     audio. EspBle carries encoded payloads, so decoding CVSD or mSBC and
//     driving a speaker belong to another library. An accessory also has things
//     to say about itself, which is the second half of this sketch.
// ja: HFPのheadset側。通話の制御経路と、encode済みのSCO音声を扱う。EspBleが運ぶのは
//     encode済みpayloadなので、CVSDやmSBCのdecodeとspeakerの駆動は別libraryの担当。
//     機器側から伝えることもあり、それがこのsketchの後半である。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

// en: Replace with the address of a phone or a test Audio Gateway.
// ja: 電話機、またはテスト用Audio Gatewayのaddressに置き換える。
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
      // en: Everything below needs the service-level connection, not just an
      //     ACL link, so it waits for this state rather than for connect().
      // ja: 以下はACL linkではなくservice-level connectionが必要なので、
      //     connect()ではなくこの状態を待つ。
      if (!bluetooth.hfpClient().serviceLevelConnected()) return;
      // en: Battery reporting only reaches a phone once the Apple extensions are
      //     enabled — Apple defined them, and Android and Windows accept them
      //     too. The identification is vendorId-productId-version in hex.
      // ja: 電池残量はApple拡張を有効にしてからでないと電話機へ届かない。Appleが
      //     定めた拡張だがAndroidやWindowsも受け付ける。identificationは
      //     vendorId-productId-versionを16進で書く。
      if (!bluetooth.hfpClient().enableAppleExtensions("0505-1995-0610"))
        Serial.printf("XAPL failed: %s\n", bluetooth.lastErrorName());
      // en: 0 is empty and 9 is full; docked means running on external power.
      // ja: 0が空、9が満充電。dockedは外部電源で動いていることを示す。
      else if (!bluetooth.hfpClient().reportBatteryLevel(7))
        Serial.printf("battery report failed: %s\n", bluetooth.lastErrorName());
      // en: An accessory with its own DSP asks the phone to stop doubling up:
      //     two noise reducers in series sound worse than one. The phone may
      //     ignore it, and there is no call to turn it back on.
      // ja: 自前でDSPを持つ機器は、電話機側の処理を止めるよう頼む。noise reduction
      //     の2段重ねは1段より悪くなる。電話機が無視することもあり、元に戻す
      //     呼び出しは無い。
      (void)bluetooth.hfpClient().disableNoiseReduction();
      // en: These two are requests, not properties: the answers arrive below.
      // ja: この2つは要求で、答えは下のcallbackへ届く。
      (void)bluetooth.hfpClient().queryOperatorName();
      (void)bluetooth.hfpClient().requestSubscriberNumber();
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
  // en: A phone is free to answer with an empty string, so the value is printed
  //     as it arrived rather than being treated as always present.
  // ja: 電話機が空文字で答えるのも正当なので、常に値があるものとして扱わず、
  //     届いたまま表示する。
  bluetooth.hfpClient().onOperatorName([](const String &name) {
    Serial.printf("network operator: %s\n", name.c_str());
  });
  bluetooth.hfpClient().onSubscriberNumber(
    [](const EspBleClassicHfpSubscriberNumber &subscriber) {
      Serial.printf("subscriber number: %s (service type %u)\n",
        subscriber.number.c_str(),
        static_cast<unsigned>(subscriber.serviceType));
    });
  // en: Whether the phone sends the ring tone over the audio link. An accessory
  //     that beeps on its own has to know, or an incoming call either rings
  //     twice or not at all. It can change between calls.
  // ja: 電話機が呼出音をaudio linkで送るかどうか。自前で鳴らす機器はこれを知る必要が
  //     ある——知らないと二重に鳴るか、まったく鳴らない。通話ごとに変わりうる。
  bluetooth.hfpClient().onInBandRingTone([](bool provided) {
    Serial.printf("in-band ring tone: %s\n", provided ? "phone" : "accessory");
  });
  bluetooth.hfpClient().onAudio(
    [](const EspBleClassicHfpEncodedAudioView &audio) {
      // en: audio.data is valid only inside this callback. Copy it into a
      //     bounded queue for a decoder; keep badFrame so an mSBC decoder can
      //     conceal the loss instead of clicking.
      // ja: audio.dataはこのcallback内でだけ有効。上限のあるqueueへcopyして
      //     decoderへ渡す。badFrameを保つことでmSBC decoderが欠損を補える——
      //     捨てるとクリックノイズになる。
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
