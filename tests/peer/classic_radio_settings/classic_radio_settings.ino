// Classic radio and link settings: transmit power, page timeout and the minimum
// encryption key size. The interesting part is not that the calls are accepted
// but that the page timeout actually shortens a connection attempt, so the
// sketch times a connect() to an address nothing answers on.
#include <EspBleClassic.h>

EspBleClassic bluetooth;

// Locally administered, so it belongs to no real device and nothing answers the
// page. The attempt therefore ends because the page timeout expired.
const char *absentAddress = "02:00:00:00:00:01";
uint32_t attemptStartedMs = 0;
bool attemptRunning = false;

void reportTxPower(const char *label)
{
  int8_t minimumDbm = 0;
  int8_t maximumDbm = 0;
  const bool read = bluetooth.txPower(minimumDbm, maximumDbm);
  Serial.printf("TX_POWER %s read=%u min=%d max=%d single=%d\n", label,
    read ? 1 : 0, minimumDbm, maximumDbm, bluetooth.txPower());
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Radio";
  if (!bluetooth.begin(config))
  {
    Serial.printf("BEGIN_FAILED error=%s\n", bluetooth.lastErrorName());
    return;
  }

  bluetooth.spp().onConnectionFailed(
    [](const EspBleClassicSppConnectionFailure &failure) {
      attemptRunning = false;
      Serial.printf("ATTEMPT_FAILED elapsed=%u peer=%s\n",
        static_cast<unsigned>(millis() - attemptStartedMs),
        failure.peerAddress.c_str());
    });
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    attemptRunning = false;
    Serial.printf("ATTEMPT_CONNECTED peer=%s\n", session.peerAddress.c_str());
  });

  Serial.println("READY");
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const String line = Serial.readStringUntil('\n');
    if (line.length() == 0) return;
    const char command = line[0];
    if (command == 'r')
    {
      reportTxPower("current");
    }
    else if (command == 'w')
    {
      // A range, a single value, and a value between two supported levels: -5
      // has to round to -6 rather than be refused.
      Serial.printf("TX_SET range=%u\n",
        bluetooth.setTxPower(-12, 9) ? 1 : 0);
      reportTxPower("range");
      Serial.printf("TX_SET single=%u\n", bluetooth.setTxPower(0) ? 1 : 0);
      reportTxPower("single");
      Serial.printf("TX_SET rounded=%u\n", bluetooth.setTxPower(-5) ? 1 : 0);
      reportTxPower("rounded");
    }
    else if (command == 'x')
    {
      Serial.printf("TX_REJECT inverted=%u error=%s\n",
        bluetooth.setTxPower(9, -12) ? 1 : 0, bluetooth.lastErrorName());
    }
    else if (command == 'p')
    {
      Serial.printf("PAGE_TIMEOUT ms=%u\n", bluetooth.pageTimeout());
    }
    else if (command == 's')
    {
      const uint16_t milliseconds =
        static_cast<uint16_t>(line.substring(1).toInt());
      Serial.printf("PAGE_SET requested=%u accepted=%u error=%s\n", milliseconds,
        bluetooth.setPageTimeout(milliseconds) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 'k')
    {
      const uint8_t bytes = static_cast<uint8_t>(line.substring(1).toInt());
      Serial.printf("KEY_SIZE requested=%u accepted=%u error=%s\n", bytes,
        bluetooth.setMinimumEncryptionKeySize(bytes) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == 'c')
    {
      attemptStartedMs = millis();
      attemptRunning = true;
      Serial.printf("ATTEMPT_STARTED requested=%u\n",
        bluetooth.spp().connect(absentAddress) ? 1 : 0);
    }
    else if (command == '?')
    {
      // Answered on demand: the boot line is printed once and is lost if the
      // serial monitor attaches after the reset.
      Serial.printf("STATE attempt=%u heap=%u\n", attemptRunning ? 1 : 0,
        static_cast<unsigned>(ESP.getFreeHeap()));
    }
  }
  delay(1);
}
