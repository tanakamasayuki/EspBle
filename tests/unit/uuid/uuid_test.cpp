// Host-side unit tests for the UUID text codec (EspBleUuid.h). The codec is
// Arduino- and stack-independent so it can be verified here with g++.
//
// The case that matters most is the short/long equivalence: "180f" and
// "0000180f-0000-1000-8000-00805f9b34fb" name the same attribute, and a
// comparison that treats them as different silently breaks every scan filter
// and UUID-addressed GATT operation.

#include "EspBleUuid.h"

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

EspBleUuidValue parse(const char *text)
{
  EspBleUuidValue value;
  espBleParseUuid(text, value);
  return value;
}

const char *format(const EspBleUuidValue &value, char *buffer)
{
  espBleFormatUuid(value, buffer, 37);
  return buffer;
}
} // namespace

int main()
{
  char text[37];

  // 16-bit expands onto the Bluetooth base UUID.
  const EspBleUuidValue battery = parse("180f");
  check("16-bit parses", battery.valid() && battery.bitSize == 16);
  check(
    "16-bit formats as 128-bit",
    std::strcmp(format(battery, text), "0000180f-0000-1000-8000-00805f9b34fb") == 0);
  check("16-bit short value", espBleUuidShortValue(battery) == 0x180f);

  // Upper case and the long spelling are the same UUID.
  check("case insensitive", espBleUuidEquals(parse("180F"), battery));
  check(
    "short equals long",
    espBleUuidEquals(parse("0000180f-0000-1000-8000-00805f9b34fb"), battery));
  check(
    "long equals short, upper case",
    espBleUuidEquals(parse("0000180F-0000-1000-8000-00805F9B34FB"), battery));

  // 32-bit works the same way.
  const EspBleUuidValue wide = parse("0000180f");
  check("32-bit parses", wide.valid() && wide.bitSize == 32);
  check("32-bit equals 16-bit of the same value", espBleUuidEquals(wide, battery));
  check("32-bit short value", espBleUuidShortValue(wide) == 0x180f);

  // A 128-bit UUID off the base range round-trips byte for byte.
  const char *custom = "5266f727-49d7-4eaf-a6f1-647570736572";
  const EspBleUuidValue vendor = parse(custom);
  check("128-bit parses", vendor.valid() && vendor.bitSize == 128);
  check("128-bit round-trips", std::strcmp(format(vendor, text), custom) == 0);
  check("128-bit short value is zero", espBleUuidShortValue(vendor) == 0);
  check("distinct UUIDs differ", !espBleUuidEquals(vendor, battery));
  check("neighbouring 16-bit UUIDs differ", !espBleUuidEquals(parse("180e"), battery));

  // Byte order: the text is most significant first, the storage least first.
  check("most significant byte last", vendor.bytes[15] == 0x52 && vendor.bytes[0] == 0x72);

  // Malformed input is rejected, never truncated or padded.
  check("empty rejected", !parse("").valid());
  check("null rejected", !parse(nullptr).valid());
  check("odd length rejected", !parse("180").valid());
  check("non-hex rejected", !parse("18zz").valid());
  check("misplaced dashes rejected", !parse("5266f72749-d7-4eaf-a6f1-647570736572").valid());
  check("missing dash rejected", !parse("5266f72749d74eaf-a6f1-6475707365726").valid());
  check("128-bit with bad digit rejected", !parse("5266f727-49d7-4eaf-a6f1-64757073657g").valid());
  check("an invalid value equals nothing", !espBleUuidEquals(parse("nope"), parse("nope")));

  // On-air UUIDs: little-endian bytes of 2, 4 or 16 length.
  const uint8_t air16[2] = {0x0f, 0x18};
  EspBleUuidValue fromAir;
  check("2-byte on-air parses", espBleUuidFromLittleEndian(air16, sizeof(air16), fromAir));
  check("2-byte on-air equals 180f", espBleUuidEquals(fromAir, battery));
  check("2-byte on-air keeps its width", fromAir.bitSize == 16);

  const uint8_t air32[4] = {0x0f, 0x18, 0x00, 0x00};
  check("4-byte on-air parses", espBleUuidFromLittleEndian(air32, sizeof(air32), fromAir));
  check("4-byte on-air equals 180f", espBleUuidEquals(fromAir, battery));

  uint8_t air128[16];
  std::memcpy(air128, vendor.bytes, sizeof(air128));
  check("16-byte on-air parses", espBleUuidFromLittleEndian(air128, sizeof(air128), fromAir));
  check("16-byte on-air round-trips", std::strcmp(format(fromAir, text), custom) == 0);

  check("3-byte on-air rejected", !espBleUuidFromLittleEndian(air128, 3, fromAir));
  check("zero-length on-air rejected", !espBleUuidFromLittleEndian(air128, 0, fromAir));
  check("null on-air rejected", !espBleUuidFromLittleEndian(nullptr, 2, fromAir));

  if (failures == 0)
  {
    std::printf("PASS uuid codec\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", failures);
  return 1;
}
