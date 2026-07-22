// Backend-independent codec for the Apple iBeacon advertising layout. An
// iBeacon is a non-connectable advertisement whose Manufacturer Specific Data
// (AD type 0xFF) carries Apple's company ID (0x004C) followed by a fixed 23-byte
// beacon structure: type (0x02), length (0x15), a 16-byte proximity UUID, a
// big-endian major and minor, and a signed measured-power (RSSI at 1 m) octet.
//
// The 25-byte value produced here is exactly what EspBleAdvertising::
// setManufacturerData() expects (company ID first), and what a scanner receives
// in EspBleScanResult::manufacturerData. This header is Arduino-independent so
// it can be verified with host unit tests (g++), matching the
// EspBleMedicalFloat.h / EspBleCgmCrc.h pattern.
#ifndef ESP_BLE_IBEACON_H
#define ESP_BLE_IBEACON_H

#include <stddef.h>
#include <stdint.h>

// Apple company identifier, little-endian on the wire (0x4C, 0x00).
static const uint8_t EspBleIBeaconCompanyIdLow = 0x4C;
static const uint8_t EspBleIBeaconCompanyIdHigh = 0x00;
static const uint8_t EspBleIBeaconType = 0x02;
static const uint8_t EspBleIBeaconDataLength = 0x15; // 21 octets after this byte

// Manufacturer Specific Data size for an iBeacon (company ID + 23-byte body).
static const size_t EspBleIBeaconManufacturerDataSize = 25;

struct EspBleIBeaconData
{
  uint8_t uuid[16] = {};   // proximity UUID, most-significant byte first
  uint16_t major = 0;
  uint16_t minor = 0;
  int8_t measuredPower = 0; // calibrated RSSI at 1 m (typically negative)
};

// Encode beacon into out (must hold EspBleIBeaconManufacturerDataSize octets).
// Returns the number of octets written (always EspBleIBeaconManufacturerDataSize).
inline size_t espBleEncodeIBeacon(const EspBleIBeaconData &beacon, uint8_t *out)
{
  out[0] = EspBleIBeaconCompanyIdLow;
  out[1] = EspBleIBeaconCompanyIdHigh;
  out[2] = EspBleIBeaconType;
  out[3] = EspBleIBeaconDataLength;
  for (size_t i = 0; i < 16; ++i)
    out[4 + i] = beacon.uuid[i];
  out[20] = static_cast<uint8_t>((beacon.major >> 8) & 0xFF);
  out[21] = static_cast<uint8_t>(beacon.major & 0xFF);
  out[22] = static_cast<uint8_t>((beacon.minor >> 8) & 0xFF);
  out[23] = static_cast<uint8_t>(beacon.minor & 0xFF);
  out[24] = static_cast<uint8_t>(beacon.measuredPower);
  return EspBleIBeaconManufacturerDataSize;
}

// True if manufacturerData holds a well-formed iBeacon (Apple company ID,
// type 0x02, length 0x15, and the full 25-octet size).
inline bool espBleIsIBeacon(const uint8_t *manufacturerData, size_t length)
{
  return manufacturerData != nullptr &&
    length >= EspBleIBeaconManufacturerDataSize &&
    manufacturerData[0] == EspBleIBeaconCompanyIdLow &&
    manufacturerData[1] == EspBleIBeaconCompanyIdHigh &&
    manufacturerData[2] == EspBleIBeaconType &&
    manufacturerData[3] == EspBleIBeaconDataLength;
}

// Decode manufacturerData into beacon. Returns false if it is not an iBeacon.
inline bool espBleDecodeIBeacon(
  const uint8_t *manufacturerData, size_t length, EspBleIBeaconData &beacon)
{
  if (!espBleIsIBeacon(manufacturerData, length))
    return false;
  for (size_t i = 0; i < 16; ++i)
    beacon.uuid[i] = manufacturerData[4 + i];
  beacon.major = static_cast<uint16_t>(
    (static_cast<uint16_t>(manufacturerData[20]) << 8) | manufacturerData[21]);
  beacon.minor = static_cast<uint16_t>(
    (static_cast<uint16_t>(manufacturerData[22]) << 8) | manufacturerData[23]);
  beacon.measuredPower = static_cast<int8_t>(manufacturerData[24]);
  return true;
}

#endif // ESP_BLE_IBEACON_H
