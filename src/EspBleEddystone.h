// Backend-independent codec for the Eddystone-URL beacon frame. Eddystone
// advertises Service Data (AD type 0x16) under the 16-bit UUID 0xFEAA. The URL
// frame body is: frame type (0x10), TX power (int8 dBm at 0 m), a URL scheme
// prefix byte, then the URL with common domain suffixes replaced by single
// expansion bytes (0x00..0x0D). This keeps a URL within the tight legacy
// advertising budget.
//
// This header is Arduino-independent so it can be verified with host unit tests
// (g++), matching the EspBleIBeacon.h / EspBleMedicalFloat.h pattern. Build the
// body here, advertise it with EspBleAdvertising::setServiceData("FEAA", ...),
// and decode it from EspBleScanResult::serviceData.
#ifndef ESP_BLE_EDDYSTONE_H
#define ESP_BLE_EDDYSTONE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Eddystone service UUID (16-bit), and the URL frame type.
static const char *const EspBleEddystoneServiceUuid = "FEAA";
static const uint8_t EspBleEddystoneUrlFrameType = 0x10;
// Max encoded URL frame body (frame + tx + scheme + up to 17 URL bytes).
static const size_t EspBleEddystoneUrlMaxBodySize = 20;

// URL scheme prefixes, indexed by the scheme byte (0x00..0x03).
static const char *const EspBleEddystoneUrlSchemes[] = {
  "http://www.", "https://www.", "http://", "https://",
};

// URL expansion codes 0x00..0x0D, expanded in place inside the URL body.
static const char *const EspBleEddystoneUrlExpansions[] = {
  ".com/", ".org/", ".edu/", ".net/", ".info/", ".biz/", ".gov/",
  ".com", ".org", ".edu", ".net", ".info", ".biz", ".gov",
};

// Encode url into an Eddystone-URL frame body written to out (outCapacity must
// be >= EspBleEddystoneUrlMaxBodySize). Sets outLength to the bytes written and
// returns true on success; returns false if the scheme is unknown or the
// encoded URL does not fit. txPower is the calibrated power at 0 m (dBm).
inline bool espBleEncodeEddystoneUrl(
  const char *url, int8_t txPower, uint8_t *out, size_t outCapacity, size_t &outLength)
{
  if (url == nullptr || out == nullptr || outCapacity < 4)
    return false;

  int scheme = -1;
  size_t schemeLength = 0;
  for (int i = 0; i < 4; ++i)
  {
    const size_t prefixLength = strlen(EspBleEddystoneUrlSchemes[i]);
    if (strncmp(url, EspBleEddystoneUrlSchemes[i], prefixLength) == 0)
    {
      // Prefer the longest matching scheme (www. variants come first).
      if (scheme < 0 || prefixLength > schemeLength)
      {
        scheme = i;
        schemeLength = prefixLength;
      }
    }
  }
  if (scheme < 0)
    return false; // Eddystone-URL requires a known http(s) scheme

  size_t length = 0;
  out[length++] = EspBleEddystoneUrlFrameType;
  out[length++] = static_cast<uint8_t>(txPower);
  out[length++] = static_cast<uint8_t>(scheme);

  const char *cursor = url + schemeLength;
  while (*cursor != '\0')
  {
    // Replace a domain suffix with its single-byte expansion code when it
    // matches at this position (longest match, ".com/" before ".com").
    int expansion = -1;
    size_t expansionLength = 0;
    for (int i = 0; i < 14; ++i)
    {
      const size_t suffixLength = strlen(EspBleEddystoneUrlExpansions[i]);
      if (strncmp(cursor, EspBleEddystoneUrlExpansions[i], suffixLength) == 0 &&
          suffixLength > expansionLength)
      {
        expansion = i;
        expansionLength = suffixLength;
      }
    }
    if (length >= outCapacity)
      return false;
    if (expansion >= 0)
    {
      out[length++] = static_cast<uint8_t>(expansion);
      cursor += expansionLength;
    }
    else
    {
      const unsigned char character = static_cast<unsigned char>(*cursor);
      // Only graphic ASCII is representable; reserved bytes would be ambiguous.
      if (character < 0x21 || character > 0x7E)
        return false;
      out[length++] = character;
      ++cursor;
    }
    if (length > EspBleEddystoneUrlMaxBodySize)
      return false;
  }

  outLength = length;
  return true;
}

// True if serviceData is an Eddystone-URL frame (frame type 0x10, min length).
inline bool espBleIsEddystoneUrl(const uint8_t *serviceData, size_t length)
{
  return serviceData != nullptr && length >= 3 &&
    serviceData[0] == EspBleEddystoneUrlFrameType && serviceData[2] < 4;
}

// Decode an Eddystone-URL frame body into urlOut (NUL-terminated, urlCapacity
// includes the terminator) and txPower. Returns false if it is not a URL frame
// or the expanded URL does not fit.
inline bool espBleDecodeEddystoneUrl(
  const uint8_t *serviceData, size_t length, char *urlOut, size_t urlCapacity, int8_t &txPower)
{
  if (!espBleIsEddystoneUrl(serviceData, length) || urlOut == nullptr || urlCapacity == 0)
    return false;

  txPower = static_cast<int8_t>(serviceData[1]);
  size_t out = 0;
  const char *scheme = EspBleEddystoneUrlSchemes[serviceData[2]];
  for (const char *s = scheme; *s != '\0'; ++s)
  {
    if (out + 1 >= urlCapacity)
      return false;
    urlOut[out++] = *s;
  }

  for (size_t i = 3; i < length; ++i)
  {
    const uint8_t byte = serviceData[i];
    if (byte < 14)
    {
      for (const char *s = EspBleEddystoneUrlExpansions[byte]; *s != '\0'; ++s)
      {
        if (out + 1 >= urlCapacity)
          return false;
        urlOut[out++] = *s;
      }
    }
    else
    {
      if (byte < 0x21 || byte > 0x7E || out + 1 >= urlCapacity)
        return false;
      urlOut[out++] = static_cast<char>(byte);
    }
  }
  urlOut[out] = '\0';
  return true;
}

#endif // ESP_BLE_EDDYSTONE_H
