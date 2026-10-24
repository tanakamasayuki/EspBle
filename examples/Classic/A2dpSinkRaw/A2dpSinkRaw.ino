// en: Receive codec configuration and encoded SBC media callbacks
// ja: codec設定とencode済みSBC mediaをcallbackで受信
#include <EspBleClassic.h>

EspBleClassic bluetooth;
volatile uint32_t packetCount = 0;
volatile uint32_t byteCount = 0;

void setup()
{
  Serial.begin(115200);

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  bluetooth.a2dpSink().onConnected(
    [](const EspBleClassicA2dpConnection &connection) {
      Serial.printf(
        "A2DP connected: %s, MTU %u\n",
        connection.peerAddress.c_str(), connection.mediaMtu);
    });
  // en: Use the negotiated codec configuration; raw encoded packets are not PCM samples and need a matching external codec pipeline.
  // ja: 交渉で決まったコーデック設定を使います。符号化パケットはPCMサンプルではないので、対応する外部コーデック処理が必要です。
  bluetooth.a2dpSink().onCodecConfigured(
    [](const EspBleClassicA2dpCodecConfig &codec) {
      Serial.printf(
        "SBC configuration: %lu Hz, %u channel(s), bitpool %u-%u\n",
        static_cast<unsigned long>(codec.sampleRate), codec.channels,
        codec.minimumBitpool, codec.maximumBitpool);
    });
  // en: This encoded audio callback runs in the Bluetooth host context. Copy borrowed payload bytes to a bounded queue before returning; decode and play them elsewhere.
  // ja: この符号化音声コールバックはBluetooth Hostの実行コンテキストで動きます。参照中のデータを容量制限付きキューへコピーして戻り、デコードや再生は別の処理で行います。
  bluetooth.a2dpSink().onMedia(
    [](const EspBleClassicEncodedAudioView &audio) {
      // en: audio.data is valid only in this callback. Copy it into a bounded queue here when a decoder such as PCMFlowBluetooth consumes it.
      // ja: audio.dataが有効なのはこのコールバック内だけです。外部デコーダで使う場合は、ここで容量制限付きキューへコピーします。
      ++packetCount;
      byteCount += audio.length;
    });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle A2DP Sink";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!bluetooth.begin(stackConfig) || !bluetooth.a2dpSink().begin())
    Serial.println(bluetooth.lastErrorDetail());
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  static uint32_t nextReport = 0;
  if (static_cast<int32_t>(millis() - nextReport) >= 0)
  {
    Serial.printf("encoded packets=%lu bytes=%lu\n",
      static_cast<unsigned long>(packetCount),
      static_cast<unsigned long>(byteCount));
    nextReport = millis() + 1000;
  }
  delay(1);
}
