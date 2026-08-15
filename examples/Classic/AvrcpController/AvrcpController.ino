// en: AVRCP Controller: the side that presses play and pause on the other
//     device. examples/Classic/A2dpSinkAvrcp shows the Target side, which
//     receives those presses. AVRCP carries control only — the audio travels
//     over A2DP, and the AVRCP connection follows the A2DP one.
// ja: AVRCP Controller。相手側の再生・停止を操作する側。Target側（押された操作を
//     受ける側）はexamples/Classic/A2dpSinkAvrcpにある。AVRCPは操作だけを運び、
//     音声はA2DPが運ぶ。AVRCPの接続はA2DP接続に追従する。
#include <EspBleClassic.h>

EspBleClassic bluetooth;

// en: Replace with the player's address, or find one with
//     examples/Classic/Inquiry.
// ja: 相手のaddressに置き換える。examples/Classic/Inquiryで探すこともできる。
const char *playerAddress = "00:00:00:00:00:00";

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic AVRCP Controller";
  if (!bluetooth.begin(config))
  {
    Serial.printf("Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.avrcp().onConnectionChanged(
    [](const EspBleClassicAvrcpConnection &connection) {
      Serial.printf("AVRCP %s %s\n",
        connection.controller ? "controller" : "target",
        connection.connected ? "connected" : "disconnected");
    });
  // en: A response to a key this side sent. accepted=0 means the other device
  //     understood the command and refused it, which is different from the
  //     command never arriving.
  // ja: こちらが送ったキーへの応答。accepted=0は相手がcommandを理解した上で
  //     拒否したという意味で、届かなかった場合とは別である。
  bluetooth.avrcp().onPassthroughResponse(
    [](const EspBleClassicAvrcpPassthroughResponse &response) {
      Serial.printf("Key %u accepted=%u\n",
        static_cast<unsigned>(response.command), response.accepted ? 1 : 0);
    });
  bluetooth.avrcp().onPlayStatus(
    [](const EspBleClassicAvrcpPlayStatus &status) {
      Serial.printf("Play status %u at %lu ms of %lu\n",
        static_cast<unsigned>(status.state),
        static_cast<unsigned long>(status.positionMilliseconds),
        static_cast<unsigned long>(status.trackLengthMilliseconds));
    });
  bluetooth.avrcp().onMetadata(
    [](const EspBleClassicAvrcpMetadata &metadata) {
      Serial.printf("Metadata attribute %u: %s\n",
        static_cast<unsigned>(metadata.attribute), metadata.value.c_str());
    });

  // en: The Controller role is the one this sketch uses, so the Target side is
  //     left off; a device can run both, as A2dpSinkAvrcp does.
  // ja: このsketchはController roleだけを使うのでTargetは無効にする。両方を
  //     動かすこともでき、A2dpSinkAvrcpがその例。
  EspBleClassicAvrcpConfig avrcpConfig;
  avrcpConfig.controller = true;
  avrcpConfig.target = false;
  // en: AVRCP is started before A2DP because the backend requires that order.
  // ja: backendの要件により、AVRCPをA2DPより先に開始する。
  if (!bluetooth.avrcp().begin(avrcpConfig) ||
      !bluetooth.a2dpSource().begin())
  {
    Serial.printf("AVRCP start failed: %s\n",
      bluetooth.lastErrorDetail().c_str());
    return;
  }

  Serial.println("Send 'c' to connect, then 'p' 'x' 'n' 'b' 'i' 'm' 'v' 'r'.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    // en: The AVRCP connection follows the A2DP one, so connecting media is
    //     what makes the control commands reachable.
    // ja: AVRCPの接続はA2DPに追従するため、mediaを接続することで操作commandが
    //     届くようになる。
    if (command == 'c') bluetooth.a2dpSource().connect(playerAddress);
    else if (command == 'p')
      bluetooth.avrcp().sendKey(EspBleClassicAvrcpCommand::Play);
    else if (command == 'x')
      bluetooth.avrcp().sendKey(EspBleClassicAvrcpCommand::Pause);
    else if (command == 'n')
      bluetooth.avrcp().sendKey(EspBleClassicAvrcpCommand::Next);
    else if (command == 'b')
      bluetooth.avrcp().sendKey(EspBleClassicAvrcpCommand::Previous);
    else if (command == 'i')
      bluetooth.avrcp().requestPlayStatus();
    else if (command == 'm')
      // en: A bit mask of the attributes to ask for: title, artist, album.
      // ja: 要求する属性のbit mask。title、artist、album。
      bluetooth.avrcp().requestMetadata(0x07);
    else if (command == 'v')
      // en: Absolute volume is 0..127, not a percentage.
      // ja: absolute volumeは0〜127で、パーセントではない。
      bluetooth.avrcp().setAbsoluteVolume(64);
    else if (command == 'r')
      // en: Repeat mode (attribute 2) set to single track (value 2). A player
      //     may refuse settings it does not implement.
      // ja: repeat mode（属性2）をsingle track（値2）にする。実装していない
      //     設定は相手が拒否することがある。
      bluetooth.avrcp().setPlayerSetting(2, 2);
  }

  bluetooth.update();
  delay(1);
}
