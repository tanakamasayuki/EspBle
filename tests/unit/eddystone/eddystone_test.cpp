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

  // Non-URL service data is rejected by the decoder.
  {
    const uint8_t uid[] = {0x00, 0xEC, 0x01, 0x02};
    char url[64];
    int8_t txPower = 0;
    check("reject non-url", !espBleDecodeEddystoneUrl(uid, sizeof(uid), url, sizeof(url), txPower));
  }

  if (failures == 0)
    std::printf("OK\n");
  return failures == 0 ? 0 : 1;
}
