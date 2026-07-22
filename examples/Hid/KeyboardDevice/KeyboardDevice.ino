// en: KeyboardDevice - turn the board into a BLE HID keyboard (HID over GATT, fixed 6KRO).
//     Paired from a PC/phone it types like a real keyboard; keystrokes are triggered by
//     Serial commands. Also receives the host's LED output report (Num/Caps/Scroll Lock).
// ja: KeyboardDevice - ボードをBLE HID keyboard（HID over GATT、固定6KRO）にする。
//     PC/スマホからPairingすると普通のキーボードとして入力でき、キー入力はSerialコマンドで発生させる。
//     Host側からのLED Output Report（Num/Caps/Scroll Lock）も受信する。
#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  // en: Configure the HID Keyboard Device profile before begin(). HID + Device
  //     Information + Battery services are composed automatically, and the HID UUID
  //     and keyboard appearance are added to advertising.
  // ja: begin() 前にHID Keyboard Device profileを構成する。HID + Device Information +
  //     Battery Serviceが自動合成され、HID UUIDとkeyboard appearanceがadvertisingへ追加される。
  auto &keyboard = ble.hidKeyboard();
  EspBleHidKeyboardConfig keyboardConfig;
  keyboardConfig.manufacturer = "EspBle";
  // en: Expose HID Boot Protocol (opt-in) so onProtocolMode() below is meaningful;
  //     leave it off (default) if you only need Report Protocol Mode.
  // ja: onProtocolMode() を意味あるものにするため Boot Protocol を有効化（opt-in）。
  //     Report Protocol Modeだけでよければ既定のoffのままにする。
  keyboardConfig.bootProtocol = true;
  keyboard.configure(keyboardConfig);
  // en: Notified when the host writes LED state (Num/Caps/Scroll Lock, etc.).
  // ja: HostがLED状態（Num/Caps/Scroll Lock等）を書き込むと通知される。
  keyboard.onOutputReport([](const EspBleHidKeyboardOutputReport &report) {
    Serial.printf(
      "Keyboard LEDs: num=%u caps=%u scroll=%u\n",
      report.numLock() ? 1 : 0,
      report.capsLock() ? 1 : 0,
      report.scrollLock() ? 1 : 0);
  });
  // en: Notified when the host switches Protocol Mode. Boot Protocol Mode (0) is used
  //     by e.g. a BIOS; input is then sent over the 8-byte Boot Keyboard Input Report
  //     automatically. No application change is needed to support boot hosts.
  // ja: HostがProtocol Modeを切り替えると通知される。Boot Protocol Mode（0）はBIOS等が使い、
  //     入力は自動的に8-byteのBoot Keyboard Input Reportで送られる。boot host対応にアプリ変更は不要。
  keyboard.onProtocolMode([](uint8_t mode, EspBleConnectionId) {
    Serial.printf("Protocol Mode: %s\n",
      mode == EspBleHidKeyboard::BootProtocolMode ? "Boot" : "Report");
  });

  EspBleConfig config;
  config.deviceName = "EspBle Keyboard";
  // en: HOGP requires an encrypted link, so enabling security is effectively required.
  // ja: HOGPは暗号化linkを要求するため、security有効化が実質必須。
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Restart advertising on each disconnect.
  // ja: 切断のたびにadvertisingを再開する。
  ble.onDisconnected([](const EspBleConnection &) {
    ble.advertising().start();
  });
  ble.advertising().setName("EspBle Keyboard");
  ble.advertising().start();

  Serial.println("Send 'a' to type Shift+A and 'r' to release all keys.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    auto &keyboard = ble.hidKeyboard();
    if (command == 'a')
    {
      // en: Input report is modifier (1 byte) + up to 6 usages. Report ID and the
      //     reserved byte are handled internally.
      // ja: Input Reportは modifier 1byte + 最大6 usage。Report IDやreserved byteは内部で処理。
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x04; // en: HID usage: Keyboard a and A / ja: HID usage: Keyboard a and A
      keyboard.sendReport(report);
    }
    else if (command == 'r')
    {
      keyboard.releaseAll(); // en: send an all-keys-released report / ja: 全キーreleaseのreportを送る
    }
  }

  // en: Output report events are delivered from this update().
  // ja: Output Reportイベントはこの update() から配送される。
  ble.update();
  delay(1);
}
