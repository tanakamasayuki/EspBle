// Host-side unit tests for the iBeacon codec (EspBleIBeacon.h). The codec is
// Arduino-independent so it can be verified here with g++. Anchored on the
// fixed Apple iBeacon layout (company ID 0x004C, type 0x02, length 0x15).

#include "EspBleIBeacon.h"

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
  EspBleIBeaconData beacon;
  const uint8_t uuid[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
  std::memcpy(beacon.uuid, uuid, 16);
  beacon.major = 0x1234;
  beacon.minor = 0xABCD;
  beacon.measuredPower = -59;

  uint8_t out[EspBleIBeaconManufacturerDataSize] = {};
  const size_t written = espBleEncodeIBeacon(beacon, out);

  check("encoded size", written == 25);
  check("company id low", out[0] == 0x4C);
  check("company id high", out[1] == 0x00);
  check("type", out[2] == 0x02);
  check("length", out[3] == 0x15);
  check("uuid copied", std::memcmp(out + 4, uuid, 16) == 0);
  check("major big-endian", out[20] == 0x12 && out[21] == 0x34);
  check("minor big-endian", out[22] == 0xAB && out[23] == 0xCD);
  check("measured power", out[24] == 0xC5); // -59 as unsigned

  check("is ibeacon", espBleIsIBeacon(out, sizeof(out)));

  // Round-trip decode.
  EspBleIBeaconData decoded;
  check("decode ok", espBleDecodeIBeacon(out, sizeof(out), decoded));
  check("decode uuid", std::memcmp(decoded.uuid, uuid, 16) == 0);
  check("decode major", decoded.major == 0x1234);
  check("decode minor", decoded.minor == 0xABCD);
  check("decode power", decoded.measuredPower == -59);

  // Rejects non-iBeacon manufacturer data.
  {
    const uint8_t notBeacon[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04};
    check("reject wrong company", !espBleIsIBeacon(notBeacon, sizeof(notBeacon)));
    EspBleIBeaconData tmp;
    check("decode rejects", !espBleDecodeIBeacon(notBeacon, sizeof(notBeacon), tmp));
  }

  // Rejects a truncated iBeacon.
  {
    check("reject short", !espBleIsIBeacon(out, 10));
  }

  if (failures == 0)
    std::printf("OK\n");
  return failures == 0 ? 0 : 1;
}
