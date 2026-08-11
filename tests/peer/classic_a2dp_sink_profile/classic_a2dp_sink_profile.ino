#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleClassicConfig config;
  config.deviceName = "EspBle A2DP Sink Profile Test";
  const bool stackStarted = bluetooth.begin(config);
  Serial.printf(
    "CLASSIC_A2DP_STACK started=%u error=%s:%s\n",
    stackStarted ? 1 : 0, bluetooth.lastErrorName(),
    bluetooth.lastErrorDetail().c_str());
  if (!stackStarted) return;

  bluetooth.a2dpSink().onMedia([](const EspBleClassicEncodedAudioView &) {});
  const bool sinkStarted = bluetooth.a2dpSink().begin();
  Serial.printf(
    "CLASSIC_A2DP_SINK started=%u initialized=%u error=%s:%s\n",
    sinkStarted ? 1 : 0, bluetooth.a2dpSink().initialized() ? 1 : 0,
    bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
  if (!sinkStarted) return;

  bluetooth.a2dpSink().end();
  Serial.printf(
    "CLASSIC_A2DP_SINK_ENDED initialized=%u\n",
    bluetooth.a2dpSink().initialized() ? 1 : 0);
  bluetooth.end();
  Serial.printf("CLASSIC_A2DP_STACK_ENDED initialized=%u\n",
    bluetooth.initialized() ? 1 : 0);
}

void loop()
{
  bluetooth.update();
  delay(1);
}
