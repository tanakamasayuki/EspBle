#include <EspBleClassic.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sessionId = 0;

void setup()
{
  Serial.begin(115200);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic SPP";
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sessionId = session.id;
    Serial.printf("SPP connected: %s\n", session.peerAddress.c_str());
  });
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
  bluetooth.update();
  delay(1);
}
