// The discoverable side. It only has to be findable, so it starts the Classic
// stack and an SPP server, which is what makes it answer an inquiry.
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Inquiry Peer";
  if (!bluetooth.begin(config) || !bluetooth.spp().startServer())
  {
    Serial.printf("INQUIRY_PEER_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf(
    "INQUIRY_PEER_READY address=%02x:%02x:%02x:%02x:%02x:%02x\n",
    address[0], address[1], address[2], address[3], address[4], address[5]);
}

void loop()
{
  bluetooth.update();
  delay(10);
}
