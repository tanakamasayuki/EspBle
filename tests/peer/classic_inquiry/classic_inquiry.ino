#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
unsigned resultCount;
unsigned completeCount;

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.inquiry().onResult([](const EspBleClassicInquiryResult &result) {
    ++resultCount;
    Serial.printf(
      "INQUIRY_RESULT address=%s name=%s cod=%u:%06x rssi=%u:%d\n",
      result.address.c_str(),
      result.name.isEmpty() ? "-" : result.name.c_str(),
      result.hasClassOfDevice ? 1 : 0,
      static_cast<unsigned>(result.classOfDevice),
      result.hasRssi ? 1 : 0, result.rssi);
  });
  bluetooth.inquiry().onComplete([](const EspBleClassicInquiryComplete &event) {
    ++completeCount;
    Serial.printf("INQUIRY_COMPLETE cancelled=%u results=%u dropped=%u\n",
      event.cancelled ? 1 : 0, resultCount,
      static_cast<unsigned>(bluetooth.inquiry().droppedResultCount()));
  });

  bluetooth.inquiry().onRemoteServices(
    [](const EspBleClassicRemoteServices &services) {
      Serial.printf("INQUIRY_SERVICES peer=%s success=%u count=%u reported=%u",
        services.peerAddress.c_str(), services.success ? 1 : 0,
        static_cast<unsigned>(services.count),
        static_cast<unsigned>(services.reportedCount));
      for (size_t index = 0; index < services.count; ++index)
        Serial.printf(" %s", services.uuids[index].c_str());
      Serial.println();
    });
  bluetooth.inquiry().onRemoteName(
    [](const EspBleClassicRemoteName &remote) {
      Serial.printf("INQUIRY_NAME peer=%s success=%u name=%s\n",
        remote.peerAddress.c_str(), remote.success ? 1 : 0,
        remote.name.isEmpty() ? "-" : remote.name.c_str());
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Inquiry Test";
  if (!bluetooth.begin(config))
  {
    Serial.printf("INQUIRY_BEGIN_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf(
    "INQUIRY_READY address=%02x:%02x:%02x:%02x:%02x:%02x\n",
    address[0], address[1], address[2], address[3], address[4], address[5]);
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 's' || command == 'l')
    {
      resultCount = 0;
      completeCount = 0;
      EspBleClassicInquiryConfig config;
      // 's' is the short scan the test waits out; 'l' is long enough to be
      // cancelled while it is still running.
      config.durationSeconds = command == 's' ? 5 : 30;
      Serial.printf("INQUIRY_START started=%u running=%u\n",
        bluetooth.inquiry().start(config) ? 1 : 0,
        bluetooth.inquiry().running() ? 1 : 0);
    }
    else if (command == 'a')
    {
      // A second start while one is running must be refused, not queued.
      EspBleClassicInquiryConfig config;
      config.durationSeconds = 5;
      const bool started = bluetooth.inquiry().start(config);
      Serial.printf("INQUIRY_RESTART started=%u error=%s\n",
        started ? 1 : 0, bluetooth.lastErrorName());
    }
    else if (command == 'x')
    {
      Serial.printf("INQUIRY_STOP requested=%u error=%s\n",
        bluetooth.inquiry().stop() ? 1 : 0, bluetooth.lastErrorName());
    }
    else if (command == 'z')
    {
      EspBleClassicInquiryConfig config;
      config.durationSeconds = 0;
      const bool started = bluetooth.inquiry().start(config);
      Serial.printf("INQUIRY_INVALID started=%u error=%s\n",
        started ? 1 : 0, bluetooth.lastErrorName());
    }
    else if (command == 'u' || command == 'n')
    {
      // The address comes from the test rather than from a scan, which is the
      // case these queries exist for: knowing an address but not what the peer
      // offers or what it calls itself.
      const String line = Serial.readStringUntil('\n');
      const bool services = command == 'u';
      Serial.printf("INQUIRY_QUERY kind=%c requested=%u error=%s\n",
        command,
        (services ? bluetooth.inquiry().requestServices(line.c_str())
                  : bluetooth.inquiry().requestName(line.c_str())) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 'U')
      // An address that cannot be parsed must be refused locally.
      Serial.printf("INQUIRY_QUERY kind=U requested=%u error=%s\n",
        bluetooth.inquiry().requestServices("not-an-address") ? 1 : 0,
        bluetooth.lastErrorName());
    else if (command == '?')
    {
      Serial.printf("INQUIRY_STATE running=%u results=%u completes=%u heap=%u\n",
        bluetooth.inquiry().running() ? 1 : 0, resultCount, completeCount,
        static_cast<unsigned>(ESP.getFreeHeap()));
    }
  }
  delay(1);
}
