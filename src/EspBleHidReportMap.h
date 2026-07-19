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

enum class EspBleHidReportKind : uint8_t
{
  Unknown = 0,
  Keyboard,
  Mouse,
  Gamepad,
  ConsumerControl,
  SystemControl,
  Vendor,
};

struct EspBleHidReportMapEntry
{
  EspBleHidReportKind kind = EspBleHidReportKind::Unknown;
  bool hasReportId = false;
  uint8_t reportId = 0;
  uint16_t inputBitLength = 0;
  bool keyboardBitmap = false;
  bool keyboardHasModifiers = false;
  uint16_t keyboardModifierBitOffset = 0;
  uint16_t keyboardBitmapBitOffset = 0;
  uint16_t keyboardBitmapBitCount = 0;
  uint16_t keyboardBitmapUsageMinimum = 0;

  size_t inputByteLength() const { return (inputBitLength + 7) / 8; }
};

struct EspBleHidReportField
{
  EspBleHidReportKind kind = EspBleHidReportKind::Unknown;
  uint8_t reportId = 0;
  uint16_t usagePage = 0;
  uint16_t usage = 0;
  int32_t logicalMin = 0;
  int32_t logicalMax = 0;
  uint16_t bitOffset = 0;
  uint8_t bitSize = 0;
  uint8_t flags = 0;
};

struct EspBleHidReportMapInfo
{
  static constexpr size_t MaxEntries = 8;
  static constexpr size_t MaxFields = 64;
  EspBleHidReportMapEntry entries[MaxEntries];
  size_t count = 0;
  EspBleHidReportField fields[MaxFields];
  size_t fieldCount = 0;

  EspBleHidReportKind kindForReportId(uint8_t reportId) const
  {
    for (size_t index = 0; index < count; ++index)
    {
      if (entries[index].reportId == reportId) return entries[index].kind;
    }
    return EspBleHidReportKind::Unknown;
  }
};

inline int32_t espBleHidReadFieldValue(
  const EspBleHidReportField &field, const uint8_t *data, size_t length)
{
  if (data == nullptr || field.bitSize == 0 || field.bitSize > 32 ||
      static_cast<size_t>(field.bitOffset) + field.bitSize > length * 8)
    return 0;
  uint32_t value = 0;
  for (uint8_t bit = 0; bit < field.bitSize; ++bit)
  {
    const size_t sourceBit = static_cast<size_t>(field.bitOffset) + bit;
    if ((data[sourceBit >> 3] & static_cast<uint8_t>(1u << (sourceBit & 7))) != 0)
      value |= static_cast<uint32_t>(1u) << bit;
  }
  if (field.logicalMin < 0 && field.bitSize < 32 &&
      (value & (static_cast<uint32_t>(1u) << (field.bitSize - 1))) != 0)
    value |= ~((static_cast<uint32_t>(1u) << field.bitSize) - 1u);
  return static_cast<int32_t>(value);
}

inline EspBleHidReportMapInfo espBleParseHidReportMap(const uint8_t *data, size_t length)
{
  EspBleHidReportMapInfo info;
  if (data == nullptr) return info;
  uint16_t usagePage = 0;
  uint32_t reportSize = 0;
  uint32_t reportCount = 0;
  int32_t logicalMin = 0;
  int32_t logicalMax = 0;
  uint8_t reportId = 0;
  bool hasReportId = false;
  uint32_t usages[16] = {};
  size_t usageCount = 0;
  uint32_t usageMinimum = 0;
  uint32_t usageMaximum = 0;
  bool haveUsageRange = false;
  EspBleHidReportKind collectionKinds[8] = {};
  size_t depth = 0;

  auto signedValue = [](uint32_t value, size_t size) -> int32_t {
    if (size == 1) return static_cast<int8_t>(value);
    if (size == 2) return static_cast<int16_t>(value);
    return static_cast<int32_t>(value);
  };
  auto localUsagePage = [&usagePage](uint32_t value) -> uint16_t {
    return value > 0xffff ? static_cast<uint16_t>(value >> 16) : usagePage;
  };
  auto localUsageId = [](uint32_t value) -> uint16_t {
    return static_cast<uint16_t>(value);
  };
  auto resetLocal = [&]() {
    usageCount = 0;
    usageMinimum = 0;
    usageMaximum = 0;
    haveUsageRange = false;
  };
  auto findEntry = [&](EspBleHidReportKind kind) -> EspBleHidReportMapEntry * {
    for (size_t index = 0; index < info.count; ++index)
      if (info.entries[index].kind == kind && info.entries[index].reportId == reportId &&
          info.entries[index].hasReportId == hasReportId) return &info.entries[index];
    if (info.count == EspBleHidReportMapInfo::MaxEntries) return nullptr;
    EspBleHidReportMapEntry &entry = info.entries[info.count++];
    entry.kind = kind;
    entry.reportId = hasReportId ? reportId : 0;
    entry.hasReportId = hasReportId;
    return &entry;
  };

  for (size_t offset = 0; offset < length;)
  {
    const uint8_t prefix = data[offset++];
    if (prefix == 0xfe)
    {
      if (offset + 2 > length) break;
      const size_t itemLength = data[offset];
      if (offset + 2 + itemLength > length) break;
      offset += 2 + itemLength;
      continue;
    }
    static const uint8_t sizes[4] = {0, 1, 2, 4};
    const size_t itemLength = sizes[prefix & 3];
    if (offset + itemLength > length) break;
    uint32_t value = 0;
    for (size_t index = 0; index < itemLength; ++index)
      value |= static_cast<uint32_t>(data[offset + index]) << (index * 8);
    offset += itemLength;
    const uint8_t type = (prefix >> 2) & 3;
    const uint8_t tag = (prefix >> 4) & 15;
    if (type == 1)
    {
      if (tag == 0) usagePage = static_cast<uint16_t>(value);
      else if (tag == 1) logicalMin = signedValue(value, itemLength);
      else if (tag == 2) logicalMax = logicalMin < 0 ? signedValue(value, itemLength) : static_cast<int32_t>(value);
      else if (tag == 7) reportSize = value;
      else if (tag == 8) { reportId = static_cast<uint8_t>(value); hasReportId = true; }
      else if (tag == 9) reportCount = value;
    }
    else if (type == 2)
    {
      if (tag == 0 && usageCount < 16) usages[usageCount++] = value;
      else if (tag == 1) { usageMinimum = value; haveUsageRange = true; }
      else if (tag == 2) usageMaximum = value;
    }
    else if (type == 0 && tag == 10)
    {
      EspBleHidReportKind kind = depth == 0 ? EspBleHidReportKind::Unknown : collectionKinds[depth - 1];
      if (value == 1)
      {
        const uint32_t applicationUsage = usageCount == 0 ? 0 : usages[0];
        const uint16_t applicationPage = localUsagePage(applicationUsage);
        const uint16_t applicationId = localUsageId(applicationUsage);
        if (applicationPage == 0x0c && applicationId == 0x01) kind = EspBleHidReportKind::ConsumerControl;
        else if (applicationPage == 0x01 && applicationId == 0x06) kind = EspBleHidReportKind::Keyboard;
        else if (applicationPage == 0x01 && applicationId == 0x02) kind = EspBleHidReportKind::Mouse;
        else if (applicationPage == 0x01 && (applicationId == 0x04 || applicationId == 0x05)) kind = EspBleHidReportKind::Gamepad;
        else if (applicationPage == 0x01 && applicationId == 0x80) kind = EspBleHidReportKind::SystemControl;
        else if (applicationPage >= 0xff00) kind = EspBleHidReportKind::Vendor;
      }
      if (depth < 8) collectionKinds[depth++] = kind;
      resetLocal();
    }
    else if (type == 0 && tag == 12)
    {
      if (depth > 0) --depth;
      resetLocal();
    }
    else if (type == 0 && tag == 8)
    {
      const EspBleHidReportKind kind = depth == 0 ? EspBleHidReportKind::Unknown : collectionKinds[depth - 1];
      if (kind != EspBleHidReportKind::Unknown)
      {
        EspBleHidReportMapEntry *entry = findEntry(kind);
        if (entry != nullptr && reportSize <= 32 && reportCount <= 2048)
        {
          const uint16_t itemOffset = entry->inputBitLength;
          const uint32_t itemBits = reportSize * reportCount;
          entry->inputBitLength = static_cast<uint16_t>(
            itemBits > static_cast<uint32_t>(0xffffu - entry->inputBitLength)
              ? 0xffffu : entry->inputBitLength + itemBits);
          const bool constant = (value & 0x01) != 0;
          const bool variable = (value & 0x02) != 0;
          if (kind == EspBleHidReportKind::Keyboard && !constant && variable &&
              reportSize == 1 && haveUsageRange && localUsagePage(usageMinimum) == 0x07)
          {
            const uint16_t minimum = localUsageId(usageMinimum);
            const uint16_t maximum = localUsageId(usageMaximum);
            if (minimum == 0xe0 && maximum >= 0xe7 && reportCount >= 8)
            {
              entry->keyboardHasModifiers = true;
              entry->keyboardModifierBitOffset = itemOffset;
            }
            else if (reportCount >= 16 && minimum <= 0x04)
            {
              entry->keyboardBitmap = true;
              entry->keyboardBitmapBitOffset = itemOffset;
              entry->keyboardBitmapBitCount = static_cast<uint16_t>(
                reportCount > 256 ? 256 : reportCount);
              entry->keyboardBitmapUsageMinimum = minimum;
            }
          }
          if (!constant && variable)
          {
            for (uint32_t index = 0; index < reportCount &&
                 info.fieldCount < EspBleHidReportMapInfo::MaxFields; ++index)
            {
              uint32_t fieldUsage = 0;
              if (index < usageCount) fieldUsage = usages[index];
              else if (haveUsageRange && usageMinimum + index <= usageMaximum)
                fieldUsage = usageMinimum + index;
              else if (usageCount != 0) fieldUsage = usages[usageCount - 1];
              EspBleHidReportField &field = info.fields[info.fieldCount++];
              field.kind = kind;
              field.reportId = hasReportId ? reportId : 0;
              field.usagePage = localUsagePage(fieldUsage);
              field.usage = localUsageId(fieldUsage);
              field.logicalMin = logicalMin;
              field.logicalMax = logicalMax;
              field.bitOffset = static_cast<uint16_t>(itemOffset + index * reportSize);
              field.bitSize = static_cast<uint8_t>(reportSize);
              field.flags = static_cast<uint8_t>(value);
            }
          }
        }
      }
      resetLocal();
    }
    else if (type == 0) resetLocal();
  }
  return info;
}

#endif // ESP_BLE_HID_REPORT_MAP_H
