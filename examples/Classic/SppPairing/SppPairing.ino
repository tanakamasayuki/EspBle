// en: Application-controlled pairing and bond management
// ja: applicationが制御するpairingとbond管理
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  // en: Register the answer paths before begin(): the stack decides during
  //     initialization whether it can ask the application at all.
  // ja: 応答経路はbegin()より前に登録する。applicationへ問い合わせられるかを
  //     初期化時に判断するため。
  bluetooth.onNumericComparisonRequested(
    [](const EspBleClassicNumericComparison &event) {
      Serial.printf(
        "Confirm %s shows %06u? (auto-accepting)\n",
        event.peerAddress.c_str(), static_cast<unsigned>(event.value));
      // en: A real product asks the user here. Answering nothing rejects the
      //     pairing once responseTimeoutMilliseconds elapses.
      // ja: 実製品ではここでユーザーへ確認する。無応答なら
      //     responseTimeoutMilliseconds経過で拒否される。
      bluetooth.confirmNumericComparison(event.peerAddress.c_str(), true);
    });

  // en: Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
  // ja: セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
  bluetooth.onSecurityChanged([](const EspBleClassicSecurityChanged &event) {
    Serial.printf(
      "Pairing with %s %s (status=%d)\n", event.peerAddress.c_str(),
      event.success ? "succeeded" : "failed", event.status);
  });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Pairing";
  config.security.enabled = true;
  // en: DisplayYesNo is the capability that produces a number both sides
  //     compare. DisplayOnly shows a passkey, KeyboardOnly types one in.
  // ja: DisplayYesNoは両者が同じ数字を比較する構成。DisplayOnlyはpasskeyを表示、
  //     KeyboardOnlyはpasskeyを入力する。
  config.security.ioCapability = EspBleClassicSecurityIoCapability::DisplayYesNo;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  if (!bluetooth.spp().startServer())
  {
    Serial.printf(
      "SPP server failed: %s\n", bluetooth.lastErrorDetail().c_str());
    return;
  }

  Serial.printf("Bonded devices: %u\n",
    static_cast<unsigned>(bluetooth.bondCount()));
  for (size_t index = 0; index < bluetooth.bondCount(); ++index)
  {
    EspBleClassicBond bond;
    if (bluetooth.bond(index, bond))
      Serial.printf("  %s\n", bond.peerAddress.c_str());
  }
}

void loop()
{
  // en: Pairing questions, results and the timeout are all driven from here.
  // ja: pairingの問い合わせ・結果・timeoutはすべてここから駆動される。
  bluetooth.update();
  delay(10);
}
