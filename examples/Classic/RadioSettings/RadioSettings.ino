// en: The three radio and link settings a Classic sketch can choose: transmit
//     power, page timeout and the minimum encryption key size. None of them
//     change what a profile does, so they are easiest to see through a
//     connection attempt: a shorter page timeout is how long connect() takes to
//     give up on a peer that is not there.
// ja: Classicのsketchが選べる無線・linkの設定3つ: 送信電力、page timeout、
//     暗号鍵の最小長。どれもprofileの動作を変えないため、接続試行で見るのが
//     一番分かりやすい。page timeoutを短くすると、居ない相手に対してconnect()が
//     諦めるまでの時間が短くなる。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

// en: An address nothing answers on, so the page timeout is what ends the
//     attempt. Replace it with a real one to see a connection instead.
// ja: 誰も応答しないaddress。試行を終わらせるのがpage timeoutになる。
//     実在のaddressに変えれば接続の側が見られる。
const char *absentAddress = "00:00:00:00:00:01";
uint32_t attemptStartedMs = 0;

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Radio";
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  // en: BR/EDR power control picks a level per packet from a range, so the range
  //     form is the honest one: -12 dBm as the floor keeps a nearby link cheap,
  //     +9 as the ceiling lets the radio reach further when it has to. The
  //     single-value form pins both ends. Levels are 3 dB apart and a value in
  //     between is rounded. This is separate from EspBle::setTxPower(), which
  //     sets the LE power.
  // ja: BR/EDRの電力制御は範囲の中からpacketごとに選ぶため、範囲で渡すのが実態に
  //     合う。下限-12 dBmは近距離のlinkを安く保ち、上限+9 dBmは必要なときに
  //     遠くまで届かせる。1つだけ渡す形は上下限を同じ値に固定する。levelは3 dB
  //     刻みで、間の値は丸められる。BLEの送信電力はEspBle::setTxPower()で別に
  //     設定する。
  if (!bluetooth.setTxPower(-12, 9))
    Serial.printf("setTxPower failed: %s\n", bluetooth.lastErrorName());

  // en: A peer that is off or out of range answers nothing, and paging it is
  //     what a connection attempt spends its time on. The default is 5120 ms;
  //     1000 ms fails four times sooner, at the cost of giving up on a peer that
  //     was merely slow to answer. It applies from the next page, so it is set
  //     before connecting.
  // ja: 電源が入っていない、あるいは圏外の相手は何も返さず、接続試行の時間は
  //     pagingに費やされる。既定は5120 msで、1000 msなら4倍早く失敗する——
  //     応答が遅れただけの相手を諦める代償はある。次のpageから効くため、接続の
  //     前に設定する。
  if (!bluetooth.setPageTimeout(1000))
    Serial.printf("setPageTimeout failed: %s\n", bluetooth.lastErrorName());

  // en: Only the local side can refuse a short encryption key, and a peer that
  //     negotiates one weakens the link for both. 16 bytes is the maximum, so it
  //     refuses everything shorter; it applies to links established afterwards.
  // ja: 短い暗号鍵を断れるのは自分側だけで、短い鍵で合意する相手はlinkを両者
  //     ぶん弱める。16 byteは最大値なので、それより短い鍵をすべて拒否する。
  //     以降に確立するlinkに効く。
  if (!bluetooth.setMinimumEncryptionKeySize(16))
    Serial.printf("setMinimumEncryptionKeySize failed: %s\n",
      bluetooth.lastErrorName());

  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    Serial.printf("Connected to %s after %u ms\n",
      session.peerAddress.c_str(),
      static_cast<unsigned>(millis() - attemptStartedMs));
  });
  bluetooth.spp().onConnectionFailed(
    [](const EspBleClassicSppConnectionFailure &failure) {
      Serial.printf("Attempt to %s gave up after %u ms: %s\n",
        failure.peerAddress.c_str(),
        static_cast<unsigned>(millis() - attemptStartedMs),
        failure.detail.c_str());
    });

  Serial.println("Send 'p' to print the settings, 'c' to try a connection.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'p')
    {
      // en: The radio reports the range it applied, which is the rounded one.
      //     The page timeout is confirmed by the backend on its own task, so it
      //     reads as 0 until the confirmation arrives — including the startup
      //     read that fills in the controller's default.
      // ja: 無線が適用した範囲——つまり丸められた値——を読み戻せる。page timeoutは
      //     backendが自分のtaskで確定させるため、確定が届くまでは0を返す。
      //     controllerの既定値を読む起動時の照会も同じである。
      int8_t minimumDbm = 0;
      int8_t maximumDbm = 0;
      if (bluetooth.txPower(minimumDbm, maximumDbm))
        Serial.printf("tx power %d..%d dBm\n", minimumDbm, maximumDbm);
      Serial.printf("page timeout %u ms\n", bluetooth.pageTimeout());
    }
    else if (command == 'c')
    {
      attemptStartedMs = millis();
      Serial.printf("connect requested=%u\n",
        bluetooth.spp().connect(absentAddress) ? 1 : 0);
    }
  }

  bluetooth.update();
  delay(1);
}
