// en: CompileSmoke - minimal sketch that only checks the library builds and links
//     on the target board. It does not initialize the BLE stack; it just includes
//     the header and prints the library version.
// ja: CompileSmoke - ライブラリが対象ボードでビルド・リンクできることだけを確認する
//     最小sketch。BLEスタックは初期化せず、ヘッダのincludeとバージョン表示のみ行う。
#include <EspBle.h>

void setup()
{
  Serial.begin(115200);
  // en: ESPBLE_VERSION_STR is the library version string from espble_version.h.
  // ja: ESPBLE_VERSION_STR は espble_version.h で定義されるバージョン文字列。
  Serial.printf("EspBle %s\n", ESPBLE_VERSION_STR);
}

void loop()
{
  // en: This sketch does nothing; the goal is just to verify the build.
  // ja: このsketchは何もしない（ビルド確認が目的）。
  delay(1000);
}
