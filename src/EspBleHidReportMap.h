#ifndef ESP_BLE_HID_REPORT_MAP_H
#define ESP_BLE_HID_REPORT_MAP_H

// Minimal HID Report Map (report descriptor) parser used by the HID Keyboard
// Host to identify a boot-compatible 6KRO keyboard input report. It is
// Arduino-independent so it can be verified by host-side unit tests
// (tests/unit/report_map).
//
// The parser looks for an Application collection of Generic Desktop /
// Keyboard usage that declares an input report containing both:
// - 8 x 1-bit variable input covering Keyboard page usages 0xe0-0xe7, and
// - at least 6 x 8-bit array input from the Keyboard page.
// Item ordering, constant padding, and other collections are ignored.

#include <stddef.h>
#include <stdint.h>

struct EspBleHidKeyboardReportMapInfo
{
  bool keyboardFound = false;
  bool hasReportId = false;
  uint8_t reportId = 0;
  bool hasLedOutput = false;
};

inline EspBleHidKeyboardReportMapInfo espBleParseKeyboardReportMap(
  const uint8_t *data,
  size_t length)
{
  EspBleHidKeyboardReportMapInfo info;
  if (data == nullptr || length == 0)
  {
    return info;
  }

  struct ReportState
  {
    bool used = false;
    uint8_t reportId = 0;
    bool hasReportId = false;
    bool modifiers = false;
    bool keyArray = false;
    bool ledOutput = false;
  };
  constexpr size_t MaxReports = 8;
  ReportState reports[MaxReports];

  // Global item state.
  uint16_t usagePage = 0;
  uint32_t reportSize = 0;
  uint32_t reportCount = 0;
  uint8_t reportId = 0;
  bool hasReportId = false;

  // Local item state (reset after each main item).
  uint16_t usageMinimum = 0;
  uint16_t usageMaximum = 0;
  uint16_t usageMinimumPage = 0;
  bool haveUsageRange = false;
  uint16_t firstUsage = 0;
  uint16_t firstUsagePage = 0;
  bool haveUsage = false;

  // Collection tracking.
  int collectionDepth = 0;
  int keyboardCollectionDepth = -1; // depth at which the keyboard Application collection was opened

  auto findReport = [&reports](uint8_t id, bool withId) -> ReportState * {
    for (ReportState &report : reports)
    {
      if (report.used && report.reportId == id && report.hasReportId == withId)
      {
        return &report;
      }
    }
    for (ReportState &report : reports)
    {
      if (!report.used)
      {
        report.used = true;
        report.reportId = id;
        report.hasReportId = withId;
        return &report;
      }
    }
    return nullptr;
  };

  size_t offset = 0;
  while (offset < length)
  {
    const uint8_t prefix = data[offset++];
    if (prefix == 0xfe)
    {
      // Long item: [0xfe][dataSize][tag][data...]
      if (offset >= length)
      {
        break;
      }
      const size_t dataSize = data[offset];
      if (offset + 2 + dataSize > length)
      {
        break;
      }
      offset += 2 + dataSize;
      continue;
    }

    static const uint8_t sizes[4] = {0, 1, 2, 4};
    const size_t dataSize = sizes[prefix & 0x03];
    if (offset + dataSize > length)
    {
      break;
    }
    uint32_t value = 0;
    for (size_t index = 0; index < dataSize; ++index)
    {
      value |= static_cast<uint32_t>(data[offset + index]) << (8 * index);
    }
    offset += dataSize;

    const uint8_t type = (prefix >> 2) & 0x03;
    const uint8_t tag = (prefix >> 4) & 0x0f;
    const bool insideKeyboard = keyboardCollectionDepth >= 0;

    if (type == 0) // Main items
    {
      if (tag == 0x0a) // Collection
      {
        ++collectionDepth;
        if (value == 0x01 && keyboardCollectionDepth < 0 && haveUsage &&
            firstUsagePage == 0x0001 && firstUsage == 0x0006)
        {
          keyboardCollectionDepth = collectionDepth;
        }
      }
      else if (tag == 0x0c) // End Collection
      {
        if (keyboardCollectionDepth == collectionDepth)
        {
          keyboardCollectionDepth = -1;
        }
        if (collectionDepth > 0)
        {
          --collectionDepth;
        }
      }
      else if (tag == 0x08 && insideKeyboard) // Input
      {
        ReportState *report = findReport(reportId, hasReportId);
        if (report != nullptr)
        {
          const bool variable = (value & 0x02) != 0;
          const bool constant = (value & 0x01) != 0;
          if (!constant && variable && reportSize == 1 && reportCount == 8 &&
              haveUsageRange && usageMinimumPage == 0x0007 &&
              usageMinimum == 0x00e0 && usageMaximum == 0x00e7)
          {
            report->modifiers = true;
          }
          if (!constant && !variable && reportSize == 8 && reportCount >= 6 &&
              usagePage == 0x0007)
          {
            report->keyArray = true;
          }
        }
      }
      else if (tag == 0x09 && insideKeyboard) // Output
      {
        if ((value & 0x01) == 0 && usagePage == 0x0008)
        {
          ReportState *report = findReport(reportId, hasReportId);
          if (report != nullptr)
          {
            report->ledOutput = true;
          }
        }
      }
      // All main items reset the local item state.
      haveUsageRange = false;
      haveUsage = false;
    }
    else if (type == 1) // Global items
    {
      switch (tag)
      {
      case 0x00: usagePage = static_cast<uint16_t>(value); break;
      case 0x07: reportSize = value; break;
      case 0x08:
        reportId = static_cast<uint8_t>(value);
        hasReportId = true;
        break;
      case 0x09: reportCount = value; break;
      default: break;
      }
    }
    else if (type == 2) // Local items
    {
      switch (tag)
      {
      case 0x00: // Usage
        if (!haveUsage)
        {
          haveUsage = true;
          firstUsage = static_cast<uint16_t>(value);
          firstUsagePage = dataSize == 4 ? static_cast<uint16_t>(value >> 16) : usagePage;
        }
        break;
      case 0x01: // Usage Minimum
        usageMinimum = static_cast<uint16_t>(value);
        usageMinimumPage = dataSize == 4 ? static_cast<uint16_t>(value >> 16) : usagePage;
        haveUsageRange = true;
        break;
      case 0x02: // Usage Maximum
        usageMaximum = static_cast<uint16_t>(value);
        break;
      default: break;
      }
    }
  }

  for (const ReportState &report : reports)
  {
    if (report.used && report.modifiers && report.keyArray)
    {
      info.keyboardFound = true;
      info.hasReportId = report.hasReportId;
      info.reportId = report.hasReportId ? report.reportId : 0;
      info.hasLedOutput = report.ledOutput;
      break;
    }
  }
  return info;
}

#endif // ESP_BLE_HID_REPORT_MAP_H
