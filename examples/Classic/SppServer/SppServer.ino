// en: Binary-safe SPP echo server
// ja: binary-safeなSPP echo server
#include <EspBleClassic.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sessionId = 0;

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic SPP";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sessionId = session.id;
    Serial.printf("SPP connected: %s\n", session.peerAddress.c_str());
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &) {
    sessionId = 0;
    Serial.println("SPP disconnected");
  });
  bluetooth.spp().onData([](const EspBleClassicSppData &event) {
    bluetooth.spp().write(event.sessionId, event.value);
  });

  if (!bluetooth.spp().startServer())
  {
    Serial.printf(
      "SPP start failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
  }
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();
  delay(1);
}
