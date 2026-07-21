// Backend-independent E2E-CRC codec for the Continuous Glucose Monitoring (CGM)
// Service. The CGM Service spec defines its End-to-End CRC as CRC-CCITT with
// polynomial 0x1021, initial value 0xFFFF, with data fed in least-significant-
// bit first (reflected). This is the CRC-16/MCRF4XX variant (reflected poly
// 0x8408, no final XOR), whose documented check value for the ASCII string
// "123456789" is 0x6F91.
//
// The CRC is appended little-endian as the final two octets of each CGM
// characteristic that carries E2E-CRC protection (Measurement, Feature, Status,
// Session Start/Run Time, and the Specific Ops Control Point). It is computed
// over all preceding octets of the characteristic value.
//
// This header is Arduino-independent so it can be verified with host unit tests
// (g++), matching the EspBleMedicalFloat.h / EspBleMidi.h pattern.
#ifndef ESP_BLE_CGM_CRC_H
#define ESP_BLE_CGM_CRC_H

#include <stddef.h>
#include <stdint.h>

// Compute the CGM E2E-CRC over length octets of data.
inline uint16_t espBleCgmCrc(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i)
  {
    crc ^= static_cast<uint16_t>(data[i]);
    for (int bit = 0; bit < 8; ++bit)
    {
      if (crc & 0x0001)
        crc = static_cast<uint16_t>((crc >> 1) ^ 0x8408); // reflected 0x1021
      else
        crc = static_cast<uint16_t>(crc >> 1);
    }
  }
  return crc;
}

// Append the little-endian E2E-CRC over the first length octets of buffer at
// buffer[length] and buffer[length + 1]. The buffer must hold length + 2 octets.
// Returns the total length including the CRC.
inline size_t espBleCgmAppendCrc(uint8_t *buffer, size_t length)
{
  const uint16_t crc = espBleCgmCrc(buffer, length);
  buffer[length] = static_cast<uint8_t>(crc & 0xFF);
  buffer[length + 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
  return length + 2;
}

// Verify a CGM characteristic value whose final two octets are a little-endian
// E2E-CRC over the preceding octets. Returns false if length < 2 or the CRC does
// not match.
inline bool espBleCgmVerifyCrc(const uint8_t *data, size_t length)
{
  if (length < 2)
    return false;
  const size_t payload = length - 2;
  const uint16_t expected = espBleCgmCrc(data, payload);
  const uint16_t actual = static_cast<uint16_t>(data[payload]) |
    (static_cast<uint16_t>(data[payload + 1]) << 8);
  return expected == actual;
}

#endif // ESP_BLE_CGM_CRC_H
