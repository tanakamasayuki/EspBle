// SPP through the Arduino Stream adapter. The session is opened by the peer with
// the session API, so what is under test is only the adapter: line reads, print,
// splitting a buffer larger than one packet, the write timeout, and flush().
#include <EspBleClassic.h>
#include <esp_mac.h>

EspBleClassic bluetooth;
EspBleClassicSppStream stream;

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    stream.attach(bluetooth.spp(), session.id);
    Serial.printf("STREAM_CONNECTED peer=%s session=%u\n",
      session.peerAddress.c_str(), static_cast<unsigned>(session.id));
  });
  bluetooth.spp().onDisconnected([](const EspBleClassicSppSession &session) {
    if (session.id == stream.session()) stream.detach();
    Serial.println("STREAM_DISCONNECTED");
  });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Stream";
  if (!bluetooth.begin(config))
  {
    Serial.printf("BEGIN_FAILED error=%s\n", bluetooth.lastErrorName());
    return;
  }

  EspBleClassicSppServerConfig serverConfig;
  serverConfig.serviceName = "EspBle Stream";
  if (!bluetooth.spp().startServer(serverConfig))
  {
    Serial.printf("SERVER_FAILED error=%s\n", bluetooth.lastErrorName());
    return;
  }

  stream.setTimeout(500);

  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf("READY address=%02x:%02x:%02x:%02x:%02x:%02x\n", address[0],
    address[1], address[2], address[3], address[4], address[5]);
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const String line = Serial.readStringUntil('\n');
    if (line.length() == 0) return;
    const char command = line[0];
    if (command == 'a')
    {
      // Answered on demand rather than only at boot: a banner printed once is
      // lost if the serial monitor attaches after the reset.
      uint8_t address[6] = {};
      esp_read_mac(address, ESP_MAC_BT);
      Serial.printf("ADDRESS %02x:%02x:%02x:%02x:%02x:%02x\n", address[0],
        address[1], address[2], address[3], address[4], address[5]);
    }
    else if (command == '?')
    {
      Serial.printf("STREAM_STATE attached=%u connected=%u session=%u avail=%d\n",
        stream.attached() ? 1 : 0, stream.connected() ? 1 : 0,
        static_cast<unsigned>(stream.session()), stream.available());
    }
    else if (command == 'l')
    {
      // println() through Print, so what is exercised is write(buffer, size).
      const size_t written = stream.println("hello stream");
      Serial.printf("STREAM_PRINT written=%u\n",
        static_cast<unsigned>(written));
    }
    else if (command == 'b')
    {
      // Larger than one SPP packet (990 bytes), so the adapter has to split it
      // and keep the order.
      static uint8_t payload[2500];
      for (size_t index = 0; index < sizeof(payload); ++index)
        payload[index] = static_cast<uint8_t>((index * 7 + 13) & 0xff);
      const size_t written = stream.write(payload, sizeof(payload));
      Serial.printf("STREAM_BULK requested=%u written=%u\n",
        static_cast<unsigned>(sizeof(payload)),
        static_cast<unsigned>(written));
    }
    else if (command == 'n')
    {
      // With no wait, a write that cannot fit reports what it took instead of
      // stalling the loop. The queue holds 8 writes, so asking for more packets
      // than that at once has to come back short.
      stream.setWriteTimeout(0);
      static uint8_t payload[990 * 12];
      memset(payload, 0x5a, sizeof(payload));
      const uint32_t startedMs = millis();
      const size_t written = stream.write(payload, sizeof(payload));
      const uint32_t elapsedMs = millis() - startedMs;
      stream.setWriteTimeout(1000);
      Serial.printf("STREAM_NOWAIT requested=%u written=%u elapsed=%u\n",
        static_cast<unsigned>(sizeof(payload)),
        static_cast<unsigned>(written), static_cast<unsigned>(elapsedMs));
    }
    else if (command == 'f')
    {
      const uint32_t startedMs = millis();
      stream.flush();
      Serial.printf("STREAM_FLUSH pending=%u elapsed=%u\n",
        static_cast<unsigned>(
          bluetooth.spp().pendingWriteCount(stream.session())),
        static_cast<unsigned>(millis() - startedMs));
    }
    else if (command == 'r')
    {
      // Serial-style reading, including the timeout Stream::setTimeout() governs.
      const String received = stream.readStringUntil('\n');
      Serial.printf("STREAM_LINE len=%u value=%s\n",
        static_cast<unsigned>(received.length()), received.c_str());
    }
    else if (command == 'i')
    {
      // parseInt() comes from Stream and needs nothing but read()/peek(), which
      // is the point of implementing the adapter rather than a lookalike.
      const long value = stream.parseInt();
      Serial.printf("STREAM_INT value=%ld\n", value);
    }
    else if (command == 'd')
    {
      stream.detach();
      Serial.printf("STREAM_DETACHED attached=%u avail=%d\n",
        stream.attached() ? 1 : 0, stream.available());
    }
  }
  delay(1);
}
