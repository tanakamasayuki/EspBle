// The discoverable side. It only has to be findable, so it starts the Classic
// stack and an SPP server, which is what makes it answer an inquiry. It also
// carries the commands for the Class of Device and visibility checks, because
// both are properties of the side being looked for.
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;

// The requested class, so the sketch can tell when the change it asked for is
// the one in effect.
EspBleClassicClassOfDevice requested;
const char *pendingEvent = nullptr;

// setClassOfDevice() only accepts the request: the backend applies it on its own
// task, and the library re-asserts it after a profile registration overwrites
// it. So a sketch that needs to know when the class is live reads it back until
// it matches, which is also what makes the value observable to a test.
void reportWhenClassOfDeviceIsLive()
{
  if (pendingEvent == nullptr) return;
  EspBleClassicClassOfDevice live;
  if (!bluetooth.classOfDevice(live)) return;
  if (live.majorDeviceClass != requested.majorDeviceClass ||
      live.minorDeviceClass != requested.minorDeviceClass ||
      live.serviceClass != requested.serviceClass)
  {
    return;
  }
  Serial.printf("INQUIRY_PEER_%s %02x:%02x:%03x\n", pendingEvent,
    live.majorDeviceClass, live.minorDeviceClass, live.serviceClass);
  pendingEvent = nullptr;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Inquiry Peer";
  // Peripheral / keyboard, which is what a Host uses to pick an icon and to
  // decide whether it offers to connect at all.
  config.classOfDevice.majorDeviceClass = 0x05;
  config.classOfDevice.minorDeviceClass = 0x10;
  requested = config.classOfDevice;
  pendingEvent = "COD_LIVE";
  if (!bluetooth.begin(config) || !bluetooth.spp().startServer())
  {
    Serial.printf("INQUIRY_PEER_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf(
    "INQUIRY_PEER_READY address=%02x:%02x:%02x:%02x:%02x:%02x visibility=%u\n",
    address[0], address[1], address[2], address[3], address[4], address[5],
    static_cast<unsigned>(bluetooth.visibility()));
}

void loop()
{
  bluetooth.update();

  reportWhenClassOfDeviceIsLive();

  if (Serial.available())
  {
    const String line = Serial.readStringUntil('\n');
    if (line.length() == 0) return;
    const char command = line[0];
    if (command == 'h')
      Serial.printf("INQUIRY_PEER_VISIBILITY changed=%u value=%u\n",
        bluetooth.setVisibility(EspBleClassicVisibility::Hidden) ? 1 : 0,
        static_cast<unsigned>(bluetooth.visibility()));
    else if (command == 'v')
      Serial.printf("INQUIRY_PEER_VISIBILITY changed=%u value=%u\n",
        bluetooth.setVisibility(
          EspBleClassicVisibility::ConnectableDiscoverable) ? 1 : 0,
        static_cast<unsigned>(bluetooth.visibility()));
    else if (command == 'c')
    {
      // Audio/Video, loudspeaker: a class the sketch chooses after begin(),
      // which is the case a composed device needs.
      EspBleClassicClassOfDevice audio;
      audio.majorDeviceClass = 0x04;
      audio.minorDeviceClass = 0x05;
      audio.serviceClass = 0x100;
      Serial.printf("INQUIRY_PEER_COD accepted=%u\n",
        bluetooth.setClassOfDevice(audio) ? 1 : 0);
      requested = audio;
      pendingEvent = "COD_CHANGED";
    }
    else if (command == 'x')
    {
      // A field that does not fit its bit width must be refused rather than
      // truncated into a different device class.
      EspBleClassicClassOfDevice invalid;
      invalid.majorDeviceClass = 0x40;
      Serial.printf("INQUIRY_PEER_COD_INVALID changed=%u error=%s\n",
        bluetooth.setClassOfDevice(invalid) ? 1 : 0,
        bluetooth.lastErrorName());
    }
  }
  delay(10);
}
