#include "../../../sketch_support/EspBleNvsFlashBootstrap.h"

void setup()
{
  EspBleNvsFlashBootstrap::begin();
}

void loop()
{
  EspBleNvsFlashBootstrap::update();
}
