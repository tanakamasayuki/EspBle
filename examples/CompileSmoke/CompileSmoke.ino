// en: Minimal build check; prints the library version
// ja: 最小のビルド確認。ライブラリバージョンを表示
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
