// Host-side unit tests for the CGM E2E-CRC codec (EspBleCgmCrc.h) used by the
// Continuous Glucose Monitoring Service. The codec is Arduino-independent so it
// can be verified here with g++. The anchor is the documented CRC-16/MCRF4XX
// check value 0x6F91 for the ASCII string "123456789".

#include "EspBleCgmCrc.h"

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
  // Documented CRC-16/MCRF4XX check value for "123456789".
  {
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    check("check value 0x6F91", espBleCgmCrc(data, sizeof(data)) == 0x6F91);
  }

  // Empty input returns the initial value.
  {
    check("empty crc", espBleCgmCrc(nullptr, 0) == 0xFFFF);
  }

  // A single zero octet.
  {
    const uint8_t data[1] = {0x00};
    // 0xFFFF processed against one 0x00 octet.
    const uint16_t crc = espBleCgmCrc(data, sizeof(data));
    // Reconstruct independently via the same reflected algorithm to guard the
    // append/verify helpers rather than hard-coding the intermediate value.
    uint8_t buffer[3] = {0x00, 0x00, 0x00};
    const size_t total = espBleCgmAppendCrc(buffer, 1);
    check("append length", total == 3);
    check("append matches crc", (static_cast<uint16_t>(buffer[1]) |
      (static_cast<uint16_t>(buffer[2]) << 8)) == crc);
  }

  // Round-trip: append then verify a representative CGM Measurement payload.
  {
    // Size(1)=6, Flags(1)=0x03, Glucose SFLOAT(2)=0x00 0x64 (100), Time Offset
    // uint16(2)=0x0005. CRC appended as octets 6..7.
    uint8_t measurement[8] = {0x06, 0x03, 0x00, 0x64, 0x05, 0x00, 0x00, 0x00};
    const size_t total = espBleCgmAppendCrc(measurement, 6);
    check("measurement total length", total == 8);
    check("measurement verifies", espBleCgmVerifyCrc(measurement, total));

    // Flipping any octet must break verification.
    measurement[3] ^= 0x01;
    check("corruption detected", !espBleCgmVerifyCrc(measurement, total));
    measurement[3] ^= 0x01;
    check("restored verifies", espBleCgmVerifyCrc(measurement, total));
  }

  // A value too short to contain a CRC does not verify.
  {
    const uint8_t tooShort[1] = {0x00};
    check("too short rejected", !espBleCgmVerifyCrc(tooShort, sizeof(tooShort)));
  }

  if (failures != 0)
  {
    std::printf("%d check(s) failed\n", failures);
    return 1;
  }
  std::puts("OK");
  return 0;
}
