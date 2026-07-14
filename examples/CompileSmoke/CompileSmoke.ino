#include <EspBle.h>

void setup()
{
  Serial.begin(115200);
  Serial.printf("EspBle %s\n", ESPBLE_VERSION_STR);
}

void loop()
{
  delay(1000);
}
