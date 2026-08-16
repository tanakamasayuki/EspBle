// DUT for the core-host HFP interoperability test. EspBle's Classic host runs
// the HFP Client; peer_device/ is an Audio Gateway written against the ESP-IDF
// Bluedroid API Arduino-ESP32 ships. The service-level connection, the
// indicators and the call-control AT exchanges therefore cross a stack
// boundary.
//
// SCO audio is out of scope: the core's HFP is built with the PCM audio path,
// so the AG side routes voice to a codec chip rather than over HCI. EspBle's
// raw SCO transport is covered against an EspBle AG in `classic_hfp_client`.
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
bool clientStarted = false;
bool serviceLevelUp = false;
bool callActive = false;
unsigned ringCount = 0;
unsigned setupState = 0;
String callerNumber;

String classicAddress()
{
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  char text[18];
  snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x", address[0],
    address[1], address[2], address[3], address[4], address[5]);
  return String(text);
}

void reportReady()
{
  Serial.printf("HFPCLIENT_READY started=%u address=%s\n", clientStarted ? 1 : 0,
    classicAddress().c_str());
  Serial.printf("HFPCLIENT_STATE slc=%u active=%u setup=%u rings=%u caller=%s\n",
    serviceLevelUp ? 1 : 0, callActive ? 1 : 0, setupState, ringCount,
    callerNumber.length() != 0 ? callerNumber.c_str() : "none");
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleClassicConfig config;
  config.deviceName = "EspBle CoreHost HFP Client";
  config.visibility = EspBleClassicVisibility::ConnectableDiscoverable;
  if (!bluetooth.begin(config))
  {
    Serial.printf("HFPCLIENT_INIT_FAILED %s:%s\n", bluetooth.lastErrorName(),
      bluetooth.lastErrorDetail().c_str());
    return;
  }

  bluetooth.hfpClient().onConnectionChanged(
    [](const EspBleClassicHfpConnection &event) {
      serviceLevelUp =
        event.state == EspBleClassicHfpConnectionState::ServiceLevelConnected;
      Serial.printf("HFPCLIENT_CONNECTION state=%u peer=%s features=%lu\n",
        static_cast<unsigned>(event.state), event.peerAddress.c_str(),
        static_cast<unsigned long>(event.peerFeatures));
    });
  bluetooth.hfpClient().onCallStateChanged(
    [](const EspBleClassicHfpCallState &state) {
      callActive = state.active;
      setupState = static_cast<unsigned>(state.setup);
      // The indicators come from the AG on the other stack; a Client that
      // misreads them reports the wrong call state here.
      Serial.printf("HFPCLIENT_CALL active=%u setup=%u held=%u\n",
        state.active ? 1 : 0, static_cast<unsigned>(state.setup),
        static_cast<unsigned>(state.held));
    });
  bluetooth.hfpClient().onCaller([](const EspBleClassicHfpCaller &caller) {
    callerNumber = caller.number;
    Serial.printf("HFPCLIENT_CALLER number=%s\n", caller.number.c_str());
  });
  bluetooth.hfpClient().onRing([]() {
    ++ringCount;
    Serial.printf("HFPCLIENT_RING count=%u\n", ringCount);
  });

  clientStarted = bluetooth.hfpClient().begin();
  reportReady();
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "?")
    {
      reportReady();
    }
    else if (command.startsWith("c"))
    {
      const bool accepted = bluetooth.hfpClient().connect(command.substring(1).c_str());
      Serial.printf("HFPCLIENT_CONNECT requested=%u error=%s:%s\n", accepted ? 1 : 0,
        bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    }
    else if (command == "a")
    {
      Serial.printf("HFPCLIENT_ANSWER requested=%u\n",
        bluetooth.hfpClient().answerCall() ? 1 : 0);
    }
    else if (command == "h")
    {
      Serial.printf("HFPCLIENT_HANGUP requested=%u\n",
        bluetooth.hfpClient().rejectOrEndCall() ? 1 : 0);
    }
    else if (command == "d")
    {
      Serial.printf("HFPCLIENT_DIAL requested=%u\n",
        bluetooth.hfpClient().dial("12345") ? 1 : 0);
    }
    else if (command == "q")
    {
      Serial.printf("HFPCLIENT_DISCONNECT requested=%u\n",
        bluetooth.hfpClient().disconnect() ? 1 : 0);
    }
  }

  delay(1);
}
