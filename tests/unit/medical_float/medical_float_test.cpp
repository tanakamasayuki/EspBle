// Host-side unit tests for the IEEE-11073 medical FLOAT/SFLOAT codec
// (EspBleMedicalFloat.h) used by standard GATT sensor services such as Health
// Thermometer (32-bit FLOAT), Blood Pressure and Glucose (16-bit SFLOAT). The
// codec is Arduino-independent so it can be verified here with g++.

#include "EspBleMedicalFloat.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

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

bool approx(double a, double b)
{
  return std::fabs(a - b) < 1e-9;
}
} // namespace

int main()
{
  // 32-bit FLOAT: 37.5 = mantissa 375, exponent -1. Little-endian bytes.
  {
    uint8_t buffer[4];
    espBleWriteMedicalFloat32LE(buffer, 375, -1);
    check("float32 bytes", buffer[0] == 0x77 && buffer[1] == 0x01 &&
      buffer[2] == 0x00 && buffer[3] == 0xFF);
    check("float32 decode", approx(espBleReadMedicalFloat32LE(buffer), 37.5));
  }

  // Negative mantissa round trips.
  {
    uint8_t buffer[4];
    espBleWriteMedicalFloat32LE(buffer, -50, -1);
    check("float32 negative", approx(espBleReadMedicalFloat32LE(buffer), -5.0));
  }

  // Exponent 0 and a larger mantissa.
  {
    uint8_t buffer[4];
    espBleWriteMedicalFloat32LE(buffer, 100000, 0);
    check("float32 exp0", approx(espBleReadMedicalFloat32LE(buffer), 100000.0));
  }

  // Special values decode to NaN / +-INFINITY.
  {
    const uint8_t nan[4] = {0xFF, 0xFF, 0x7F, 0x00};       // mantissa 0x7FFFFF
    const uint8_t nres[4] = {0x00, 0x00, 0x80, 0x00};      // mantissa 0x800000
    const uint8_t posInf[4] = {0xFE, 0xFF, 0x7F, 0x00};    // mantissa 0x7FFFFE
    const uint8_t negInf[4] = {0x02, 0x00, 0x80, 0x00};    // mantissa 0x800002
    check("float32 nan", std::isnan(espBleReadMedicalFloat32LE(nan)));
    check("float32 nres", std::isnan(espBleReadMedicalFloat32LE(nres)));
    check("float32 +inf", std::isinf(espBleReadMedicalFloat32LE(posInf)) &&
      espBleReadMedicalFloat32LE(posInf) > 0);
    check("float32 -inf", std::isinf(espBleReadMedicalFloat32LE(negInf)) &&
      espBleReadMedicalFloat32LE(negInf) < 0);
  }

  // 16-bit SFLOAT: 37.2 = mantissa 372, exponent -1.
  {
    uint8_t buffer[2];
    espBleWriteMedicalSFloatLE(buffer, 372, -1);
    // packed = (exponent << 12) | (mantissa & 0x0FFF) = (0xF << 12) | 0x174 = 0xF174
    check("sfloat bytes", buffer[0] == 0x74 && buffer[1] == 0xF1);
    check("sfloat decode", approx(espBleReadMedicalSFloatLE(buffer), 37.2));
  }

  // SFLOAT negative mantissa round trips.
  {
    uint8_t buffer[2];
    espBleWriteMedicalSFloatLE(buffer, -125, -1);
    check("sfloat negative", approx(espBleReadMedicalSFloatLE(buffer), -12.5));
  }

  // SFLOAT special values.
  {
    const uint8_t nan[2] = {0xFF, 0x07};    // mantissa 0x7FF
    const uint8_t posInf[2] = {0xFE, 0x07};  // mantissa 0x7FE
    const uint8_t negInf[2] = {0x02, 0x08};  // mantissa 0x802
    check("sfloat nan", std::isnan(espBleReadMedicalSFloatLE(nan)));
    check("sfloat +inf", std::isinf(espBleReadMedicalSFloatLE(posInf)) &&
      espBleReadMedicalSFloatLE(posInf) > 0);
    check("sfloat -inf", std::isinf(espBleReadMedicalSFloatLE(negInf)) &&
      espBleReadMedicalSFloatLE(negInf) < 0);
  }

  if (failures != 0)
  {
    std::printf("%d check(s) failed\n", failures);
    return 1;
  }
  std::puts("OK");
  return 0;
}
