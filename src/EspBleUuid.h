#pragma once

// Bluetooth UUID text codec, independent of Arduino and of the BLE stack so it
// can be unit tested on the host.
//
// A 16-bit UUID is shorthand for a 128-bit one on the Bluetooth base UUID
// (0000xxxx-0000-1000-8000-00805f9b34fb), so "180f" and its long spelling name
// the same attribute. Everything here therefore keeps the expanded 128-bit
// value and compares at that width; the original width is kept alongside only
// because the stack wants the short form back when one was given.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct EspBleUuidValue
{
  // 128-bit value, least significant byte first (the order NimBLE's
  // ble_uuid128_t uses).
  uint8_t bytes[16] = {};
  // 16, 32 or 128. Zero means "not set".
  uint8_t bitSize = 0;

  bool valid() const { return bitSize != 0; }
};

inline int espBleHexDigitValue(char character)
{
  if (character >= '0' && character <= '9') return character - '0';
  if (character >= 'a' && character <= 'f') return character - 'a' + 10;
  if (character >= 'A' && character <= 'F') return character - 'A' + 10;
  return -1;
}

// Accepted spellings are the ones the Bluetooth SIG uses: 4 hex digits
// (16-bit), 8 hex digits (32-bit), and the dashed 36-character 128-bit form.
// Anything else is rejected rather than silently truncated or padded.
inline bool espBleParseUuid(const char *text, EspBleUuidValue &out)
{
  out = EspBleUuidValue();
  if (text == nullptr) return false;
  const size_t length = strlen(text);

  if (length == 4 || length == 8)
  {
    uint32_t value = 0;
    for (size_t index = 0; index < length; ++index)
    {
      const int digit = espBleHexDigitValue(text[index]);
      if (digit < 0) return false;
      value = (value << 4) | static_cast<uint32_t>(digit);
    }
    static const uint8_t base[16] = {
      0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
      0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(out.bytes, base, sizeof(base));
    out.bytes[12] = static_cast<uint8_t>(value & 0xff);
    out.bytes[13] = static_cast<uint8_t>((value >> 8) & 0xff);
    out.bytes[14] = static_cast<uint8_t>((value >> 16) & 0xff);
    out.bytes[15] = static_cast<uint8_t>((value >> 24) & 0xff);
    out.bitSize = length == 4 ? 16 : 32;
    return true;
  }

  if (length == 36)
  {
    // 8-4-4-4-12, most significant byte first in text.
    size_t position = 0;
    for (size_t byteIndex = 0; byteIndex < 16; ++byteIndex)
    {
      if (position == 8 || position == 13 || position == 18 || position == 23)
      {
        if (text[position] != '-') return false;
        ++position;
      }
      const int high = espBleHexDigitValue(text[position]);
      const int low = espBleHexDigitValue(text[position + 1]);
      if (high < 0 || low < 0) return false;
      out.bytes[15 - byteIndex] = static_cast<uint8_t>((high << 4) | low);
      position += 2;
    }
    if (position != 36) return false;
    out.bitSize = 128;
    return true;
  }

  return false;
}

// Always the 128-bit spelling, so one text form is comparable everywhere. The
// buffer needs 37 bytes (36 characters plus the terminator).
inline void espBleFormatUuid(const EspBleUuidValue &value, char *out, size_t size)
{
  if (out == nullptr || size == 0) return;
  const uint8_t *bytes = value.bytes;
  snprintf(
    out, size,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    bytes[15], bytes[14], bytes[13], bytes[12], bytes[11], bytes[10], bytes[9], bytes[8],
    bytes[7], bytes[6], bytes[5], bytes[4], bytes[3], bytes[2], bytes[1], bytes[0]);
}

// Compares the expanded values, so a short UUID equals its long spelling.
inline bool espBleUuidEquals(const EspBleUuidValue &left, const EspBleUuidValue &right)
{
  return left.valid() && right.valid() && memcmp(left.bytes, right.bytes, 16) == 0;
}

// 16- or 32-bit value of a UUID that has one; zero otherwise.
inline uint32_t espBleUuidShortValue(const EspBleUuidValue &value)
{
  if (value.bitSize != 16 && value.bitSize != 32) return 0;
  return static_cast<uint32_t>(value.bytes[12]) |
    (static_cast<uint32_t>(value.bytes[13]) << 8) |
    (static_cast<uint32_t>(value.bytes[14]) << 16) |
    (static_cast<uint32_t>(value.bytes[15]) << 24);
}
