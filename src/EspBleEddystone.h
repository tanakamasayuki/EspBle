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
static const uint8_t EspBleEddystoneUidFrameType = 0x00;
static const uint8_t EspBleEddystoneUrlFrameType = 0x10;
static const uint8_t EspBleEddystoneTlmFrameType = 0x20;
// Max encoded URL frame body (frame + tx + scheme + up to 17 URL bytes).
static const size_t EspBleEddystoneUrlMaxBodySize = 20;
// UID frame body: frame + tx + 10-byte namespace + 6-byte instance (RFU omitted).
static const size_t EspBleEddystoneUidBodySize = 18;
// TLM frame body (unencrypted, version 0x00): 14 bytes.
static const size_t EspBleEddystoneTlmBodySize = 14;

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

// ---- UID frame: a stable namespace (10 bytes) + instance (6 bytes) id. ----

struct EspBleEddystoneUidData
{
  uint8_t namespaceId[10] = {};
  uint8_t instanceId[6] = {};
  int8_t txPower = 0; // calibrated power at 0 m (dBm)
};

// Encode uid into out (outCapacity must be >= EspBleEddystoneUidBodySize).
// Sets outLength and returns true, or false if it does not fit.
inline bool espBleEncodeEddystoneUid(
  const EspBleEddystoneUidData &uid, uint8_t *out, size_t outCapacity, size_t &outLength)
{
  if (out == nullptr || outCapacity < EspBleEddystoneUidBodySize)
    return false;
  out[0] = EspBleEddystoneUidFrameType;
  out[1] = static_cast<uint8_t>(uid.txPower);
  for (size_t i = 0; i < 10; ++i)
    out[2 + i] = uid.namespaceId[i];
  for (size_t i = 0; i < 6; ++i)
    out[12 + i] = uid.instanceId[i];
  outLength = EspBleEddystoneUidBodySize;
  return true;
}

inline bool espBleIsEddystoneUid(const uint8_t *serviceData, size_t length)
{
  return serviceData != nullptr && length >= EspBleEddystoneUidBodySize &&
    serviceData[0] == EspBleEddystoneUidFrameType;
}

inline bool espBleDecodeEddystoneUid(
  const uint8_t *serviceData, size_t length, EspBleEddystoneUidData &uid)
{
  if (!espBleIsEddystoneUid(serviceData, length))
    return false;
  uid.txPower = static_cast<int8_t>(serviceData[1]);
  for (size_t i = 0; i < 10; ++i)
    uid.namespaceId[i] = serviceData[2 + i];
  for (size_t i = 0; i < 6; ++i)
    uid.instanceId[i] = serviceData[12 + i];
  return true;
}

// ---- TLM frame (unencrypted, version 0x00): beacon telemetry. ----

struct EspBleEddystoneTlmData
{
  uint16_t batteryMilliVolts = 0;
  float temperatureCelsius = 0.0f;   // 8.8 fixed-point on the wire
  uint32_t advertisingCount = 0;
  uint32_t uptimeDeciseconds = 0;    // time since power-on in 0.1 s units
};

// Encode tlm into out (outCapacity must be >= EspBleEddystoneTlmBodySize), all
// multi-byte fields big-endian. Sets outLength and returns true, or false if it
// does not fit.
inline bool espBleEncodeEddystoneTlm(
  const EspBleEddystoneTlmData &tlm, uint8_t *out, size_t outCapacity, size_t &outLength)
{
  if (out == nullptr || outCapacity < EspBleEddystoneTlmBodySize)
    return false;
  // Round-to-nearest 8.8 fixed-point temperature.
  const float scaled = tlm.temperatureCelsius * 256.0f;
  const int32_t fixed = static_cast<int32_t>(scaled >= 0 ? scaled + 0.5f : scaled - 0.5f);
  const int16_t temperature = static_cast<int16_t>(fixed);

  out[0] = EspBleEddystoneTlmFrameType;
  out[1] = 0x00; // version
  out[2] = static_cast<uint8_t>((tlm.batteryMilliVolts >> 8) & 0xFF);
  out[3] = static_cast<uint8_t>(tlm.batteryMilliVolts & 0xFF);
  out[4] = static_cast<uint8_t>((temperature >> 8) & 0xFF);
  out[5] = static_cast<uint8_t>(temperature & 0xFF);
  out[6] = static_cast<uint8_t>((tlm.advertisingCount >> 24) & 0xFF);
  out[7] = static_cast<uint8_t>((tlm.advertisingCount >> 16) & 0xFF);
  out[8] = static_cast<uint8_t>((tlm.advertisingCount >> 8) & 0xFF);
  out[9] = static_cast<uint8_t>(tlm.advertisingCount & 0xFF);
  out[10] = static_cast<uint8_t>((tlm.uptimeDeciseconds >> 24) & 0xFF);
  out[11] = static_cast<uint8_t>((tlm.uptimeDeciseconds >> 16) & 0xFF);
  out[12] = static_cast<uint8_t>((tlm.uptimeDeciseconds >> 8) & 0xFF);
  out[13] = static_cast<uint8_t>(tlm.uptimeDeciseconds & 0xFF);
  outLength = EspBleEddystoneTlmBodySize;
  return true;
}

inline bool espBleIsEddystoneTlm(const uint8_t *serviceData, size_t length)
{
  return serviceData != nullptr && length >= EspBleEddystoneTlmBodySize &&
    serviceData[0] == EspBleEddystoneTlmFrameType && serviceData[1] == 0x00;
}

inline bool espBleDecodeEddystoneTlm(
  const uint8_t *serviceData, size_t length, EspBleEddystoneTlmData &tlm)
{
  if (!espBleIsEddystoneTlm(serviceData, length))
    return false;
  tlm.batteryMilliVolts = static_cast<uint16_t>(
    (static_cast<uint16_t>(serviceData[2]) << 8) | serviceData[3]);
  const int16_t temperature = static_cast<int16_t>(
    (static_cast<uint16_t>(serviceData[4]) << 8) | serviceData[5]);
  tlm.temperatureCelsius = static_cast<float>(temperature) / 256.0f;
  tlm.advertisingCount =
    (static_cast<uint32_t>(serviceData[6]) << 24) |
    (static_cast<uint32_t>(serviceData[7]) << 16) |
    (static_cast<uint32_t>(serviceData[8]) << 8) |
    static_cast<uint32_t>(serviceData[9]);
  tlm.uptimeDeciseconds =
    (static_cast<uint32_t>(serviceData[10]) << 24) |
    (static_cast<uint32_t>(serviceData[11]) << 16) |
    (static_cast<uint32_t>(serviceData[12]) << 8) |
    static_cast<uint32_t>(serviceData[13]);
  return true;
}

#endif // ESP_BLE_EDDYSTONE_H
