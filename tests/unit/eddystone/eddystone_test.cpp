// Host-side unit tests for the Eddystone-URL codec (EspBleEddystone.h). The
// codec is Arduino-independent so it can be verified here with g++. Anchored on
// the Eddystone-URL frame layout (frame type 0x10, scheme byte, expansion codes)
// and the canonical "https://www.example.com/" encoding from the spec.

#include "EspBleEddystone.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
int failures = 0;

void check(const char *name, bool condition)
{
  if (!condition)
  {
    std::printf("FAIL %s\n", name);
    ++failures;
  }
}
} // namespace

int main()
{
  // Canonical example: "https://www.example.com/" with tx power -20.
  {
    uint8_t body[EspBleEddystoneUrlMaxBodySize];
    size_t length = 0;
    const bool ok = espBleEncodeEddystoneUrl(
      "https://www.example.com/", -20, body, sizeof(body), length);
    check("encode ok", ok);
    // 0x10 frame, 0xEC (-20), 0x01 scheme (https://www.), "example", 0x00 (.com/)
    check("frame type", body[0] == 0x10);
    check("tx power", body[1] == 0xEC);
    check("scheme https www", body[2] == 0x01);
    check("literal example",
      length == 3 + 7 + 1 && std::memcmp(body + 3, "example", 7) == 0);
    check("expansion .com/", body[length - 1] == 0x00);

    check("is eddystone url", espBleIsEddystoneUrl(body, length));

    char url[64];
    int8_t txPower = 0;
    check("decode ok", espBleDecodeEddystoneUrl(body, length, url, sizeof(url), txPower));
    check("decode tx", txPower == -20);
    check("decode url", std::strcmp(url, "https://www.example.com/") == 0);
  }

  // Each scheme prefix round-trips.
  {
    const char *urls[] = {
      "http://www.a.org",   // .org expansion (no slash)
      "https://b.net/",     // .net/ expansion
      "http://c.edu/x",     // expansion mid-URL then literal
    };
    for (const char *original : urls)
    {
      uint8_t body[EspBleEddystoneUrlMaxBodySize];
      size_t length = 0;
      char url[64];
      int8_t txPower = 0;
      check("roundtrip encode", espBleEncodeEddystoneUrl(original, 0, body, sizeof(body), length));
      check("roundtrip decode", espBleDecodeEddystoneUrl(body, length, url, sizeof(url), txPower));
      check("roundtrip equal", std::strcmp(url, original) == 0);
    }
  }

  // Unknown scheme is rejected.
  {
    uint8_t body[EspBleEddystoneUrlMaxBodySize];
    size_t length = 0;
    check("reject scheme", !espBleEncodeEddystoneUrl("ftp://x.com", 0, body, sizeof(body), length));
  }

  // A URL that does not fit is rejected.
  {
    uint8_t body[EspBleEddystoneUrlMaxBodySize];
    size_t length = 0;
    check("reject too long",
      !espBleEncodeEddystoneUrl(
        "https://averylongdomainnamethatwillnotfit.example", 0, body, sizeof(body), length));
  }

  // Non-URL service data is rejected by the URL decoder.
  {
    const uint8_t frame[] = {0x00, 0xEC, 0x01, 0x02};
    char url[64];
    int8_t txPower = 0;
    check("reject non-url", !espBleDecodeEddystoneUrl(frame, sizeof(frame), url, sizeof(url), txPower));
  }

  // UID frame round-trip.
  {
    EspBleEddystoneUidData uid;
    for (int i = 0; i < 10; ++i) uid.namespaceId[i] = static_cast<uint8_t>(0xA0 + i);
    for (int i = 0; i < 6; ++i) uid.instanceId[i] = static_cast<uint8_t>(0x10 + i);
    uid.txPower = -18;

    uint8_t body[EspBleEddystoneUidBodySize];
    size_t length = 0;
    check("uid encode", espBleEncodeEddystoneUid(uid, body, sizeof(body), length));
    check("uid length", length == 18);
    check("uid frame type", body[0] == 0x00);
    check("uid tx", body[1] == 0xEE); // -18
    check("uid is", espBleIsEddystoneUid(body, length));
    check("uid not url", !espBleIsEddystoneUrl(body, length));

    EspBleEddystoneUidData decoded;
    check("uid decode", espBleDecodeEddystoneUid(body, length, decoded));
    check("uid namespace", std::memcmp(decoded.namespaceId, uid.namespaceId, 10) == 0);
    check("uid instance", std::memcmp(decoded.instanceId, uid.instanceId, 6) == 0);
    check("uid tx decode", decoded.txPower == -18);
  }

  // TLM frame round-trip (temperature 25.5 is exact in 8.8 fixed-point).
  {
    EspBleEddystoneTlmData tlm;
    tlm.batteryMilliVolts = 3300;
    tlm.temperatureCelsius = 25.5f;
    tlm.advertisingCount = 0x00010203;
    tlm.uptimeDeciseconds = 0x0A0B0C0D;

    uint8_t body[EspBleEddystoneTlmBodySize];
    size_t length = 0;
    check("tlm encode", espBleEncodeEddystoneTlm(tlm, body, sizeof(body), length));
    check("tlm length", length == 14);
    check("tlm frame type", body[0] == 0x20);
    check("tlm version", body[1] == 0x00);
    check("tlm battery be", body[2] == 0x0C && body[3] == 0xE4); // 3300
    check("tlm temp be", body[4] == 0x19 && body[5] == 0x80);    // 25.5 -> 0x1980
    check("tlm is", espBleIsEddystoneTlm(body, length));

    EspBleEddystoneTlmData decoded;
    check("tlm decode", espBleDecodeEddystoneTlm(body, length, decoded));
    check("tlm battery", decoded.batteryMilliVolts == 3300);
    check("tlm temp", decoded.temperatureCelsius == 25.5f);
    check("tlm count", decoded.advertisingCount == 0x00010203);
    check("tlm uptime", decoded.uptimeDeciseconds == 0x0A0B0C0D);
  }

  // Negative temperature round-trip (-0.5 -> 0xFF80).
  {
    EspBleEddystoneTlmData tlm;
    tlm.temperatureCelsius = -0.5f;
    uint8_t body[EspBleEddystoneTlmBodySize];
    size_t length = 0;
    espBleEncodeEddystoneTlm(tlm, body, sizeof(body), length);
    check("tlm neg temp be", body[4] == 0xFF && body[5] == 0x80);
    EspBleEddystoneTlmData decoded;
    espBleDecodeEddystoneTlm(body, length, decoded);
    check("tlm neg temp", decoded.temperatureCelsius == -0.5f);
  }

  if (failures == 0)
    std::printf("OK\n");
  return failures == 0 ? 0 : 1;
}
