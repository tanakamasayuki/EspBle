#ifndef ESP_BLE_MEDICAL_FLOAT_H
#define ESP_BLE_MEDICAL_FLOAT_H

// IEEE-11073-20601 medical FLOAT (32-bit) and SFLOAT (16-bit) codec used by
// standard GATT sensor services: Health Thermometer Temperature Measurement is
// a 32-bit FLOAT, while Blood Pressure, Glucose and Pulse Oximeter use 16-bit
// SFLOAT. Both encode a value as mantissa x 10^exponent (base-10 exponent).
//
// FLOAT (32-bit): exponent = signed 8-bit (byte 3), mantissa = signed 24-bit
// (bytes 0..2), little-endian on the wire.
// SFLOAT (16-bit): exponent = signed 4-bit (high nibble), mantissa = signed
// 12-bit, little-endian.
//
// The codec is Arduino-independent so it can be verified by host-side unit
// tests (tests/unit/medical_float). Special reserved values decode to NaN or
// +/- infinity.

#include <math.h>
#include <stdint.h>

// ---- 32-bit FLOAT ----

// Pack a FLOAT from a 24-bit signed mantissa and base-10 exponent.
inline uint32_t espBleMedicalFloat32(int32_t mantissa, int8_t exponent)
{
  return (static_cast<uint32_t>(static_cast<uint8_t>(exponent)) << 24) |
         (static_cast<uint32_t>(mantissa) & 0x00FFFFFFu);
}

inline void espBleWriteMedicalFloat32LE(uint8_t *out, int32_t mantissa, int8_t exponent)
{
  const uint32_t value = espBleMedicalFloat32(mantissa, exponent);
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

// Decode a 4-byte little-endian FLOAT. Reserved values return NaN / +-INFINITY.
inline double espBleReadMedicalFloat32LE(const uint8_t *data)
{
  const uint32_t rawMantissa =
    static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
    (static_cast<uint32_t>(data[2]) << 16);
  const int8_t exponent = static_cast<int8_t>(data[3]);
  switch (rawMantissa)
  {
  case 0x7FFFFFu: // NaN
  case 0x800000u: // NRes (not at this resolution)
  case 0x800001u: // reserved
    return NAN;
  case 0x7FFFFEu:
    return INFINITY;
  case 0x800002u:
    return -INFINITY;
  default:
    break;
  }
  int32_t mantissa = static_cast<int32_t>(rawMantissa);
  if (mantissa & 0x800000)
    mantissa -= 0x1000000; // sign-extend 24-bit
  return static_cast<double>(mantissa) * pow(10.0, static_cast<double>(exponent));
}

// ---- 16-bit SFLOAT ----

inline uint16_t espBleMedicalSFloat(int16_t mantissa, int8_t exponent)
{
  return static_cast<uint16_t>((static_cast<uint16_t>(exponent & 0x0F) << 12) |
                               (static_cast<uint16_t>(mantissa) & 0x0FFFu));
}

inline void espBleWriteMedicalSFloatLE(uint8_t *out, int16_t mantissa, int8_t exponent)
{
  const uint16_t value = espBleMedicalSFloat(mantissa, exponent);
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

inline double espBleReadMedicalSFloatLE(const uint8_t *data)
{
  const uint16_t value = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
  const uint16_t rawMantissa = value & 0x0FFF;
  int exponent = (value >> 12) & 0x0F;
  if (exponent >= 8)
    exponent -= 16; // sign-extend 4-bit
  switch (rawMantissa)
  {
  case 0x07FF: // NaN
  case 0x0800: // NRes
  case 0x0801: // reserved
    return NAN;
  case 0x07FE:
    return INFINITY;
  case 0x0802:
    return -INFINITY;
  default:
    break;
  }
  int mantissa = static_cast<int>(rawMantissa);
  if (mantissa & 0x0800)
    mantissa -= 0x1000; // sign-extend 12-bit
  return static_cast<double>(mantissa) * pow(10.0, static_cast<double>(exponent));
}

#endif // ESP_BLE_MEDICAL_FLOAT_H
