// The composed Report Descriptor is wire format: a host that paired once
// parses the descriptor it stored. These bytes are pinned so a refactor that
// reorders or renumbers anything fails here instead of on a user's desk.
#include "EspBleHidProfile.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
int failures;
void check(const char *name, bool value) {
  if (!value) { std::printf("FAIL %s\n", name); ++failures; }
}
}

int main()
{
  uint8_t descriptor[512];

  // Keyboard alone, as the KeyboardDevice example configures it.
  EspBleHidDescriptorSelection keyboardOnly;
  keyboardOnly.profileMask = 1u << ESPBLE_HID_PROFILE_KEYBOARD;
  const size_t keyboardLength =
    espBleHidComposeDescriptor(keyboardOnly, descriptor, sizeof(descriptor));
  check("keyboard descriptor length",
    keyboardLength == sizeof(EspBleHidKeyboardDescriptor));
  check("keyboard descriptor bytes",
    memcmp(descriptor, EspBleHidKeyboardDescriptor, keyboardLength) == 0);
  check("keyboard report id", descriptor[7] == ESPBLE_HID_REPORT_ID_KEYBOARD);

  // NKRO replaces the keyboard descriptor rather than adding one, so the
  // report ID and the collection count stay the same.
  EspBleHidDescriptorSelection nkro = keyboardOnly;
  nkro.keyboardNkro = true;
  const size_t nkroLength =
    espBleHidComposeDescriptor(nkro, descriptor, sizeof(descriptor));
  check("nkro descriptor length",
    nkroLength == sizeof(EspBleHidNkroKeyboardDescriptor));
  check("nkro report id", descriptor[7] == ESPBLE_HID_REPORT_ID_KEYBOARD);
  check("nkro declares a 224-bit usage bitmap",
    descriptor[nkroLength - 4] == 0xe0 && descriptor[nkroLength - 3] == 0x81);

  // A composite device concatenates in profile order, and every built-in
  // profile keeps its reserved report ID.
  EspBleHidDescriptorSelection composite;
  composite.profileMask =
    static_cast<uint8_t>((1u << ESPBLE_HID_PROFILE_KEYBOARD) |
                         (1u << ESPBLE_HID_PROFILE_MOUSE) |
                         (1u << ESPBLE_HID_PROFILE_GAMEPAD) |
                         (1u << ESPBLE_HID_PROFILE_CONSUMER) |
                         (1u << ESPBLE_HID_PROFILE_SYSTEM) |
                         (1u << ESPBLE_HID_PROFILE_VENDOR));
  const size_t compositeLength =
    espBleHidComposeDescriptor(composite, descriptor, sizeof(descriptor));
  check("composite descriptor length",
    compositeLength == sizeof(EspBleHidKeyboardDescriptor) +
      sizeof(EspBleHidMouseDescriptor) + sizeof(EspBleHidGamepadDescriptor) +
      sizeof(EspBleHidConsumerDescriptor) + sizeof(EspBleHidSystemDescriptor) +
      sizeof(EspBleHidVendorDescriptorTemplate));

  const uint8_t expectedIds[] = {
    ESPBLE_HID_REPORT_ID_KEYBOARD, ESPBLE_HID_REPORT_ID_MOUSE,
    ESPBLE_HID_REPORT_ID_GAMEPAD, ESPBLE_HID_REPORT_ID_CONSUMER,
    ESPBLE_HID_REPORT_ID_SYSTEM, ESPBLE_HID_REPORT_ID_VENDOR};
  size_t found = 0;
  for (size_t i = 0; i + 1 < compositeLength; ++i)
  {
    if (descriptor[i] != 0x85) continue;
    check("report ids appear in profile order",
      found < sizeof(expectedIds) && descriptor[i + 1] == expectedIds[found]);
    ++found;
  }
  check("every profile declared a report id", found == sizeof(expectedIds));

  // The mouse button count patches three places; a partial patch would leave
  // the report a broken number of bits.
  EspBleHidDescriptorSelection mouse;
  mouse.profileMask = 1u << ESPBLE_HID_PROFILE_MOUSE;
  mouse.mouseButtonCount = 3;
  const size_t mouseLength =
    espBleHidComposeDescriptor(mouse, descriptor, sizeof(descriptor));
  check("mouse descriptor length", mouseLength == sizeof(EspBleHidMouseDescriptor));
  check("mouse usage maximum patched",
    descriptor[ESPBLE_HID_MOUSE_USAGE_MAXIMUM_OFFSET] == 3);
  check("mouse report count patched",
    descriptor[ESPBLE_HID_MOUSE_BUTTON_COUNT_OFFSET] == 3);
  check("mouse padding keeps the byte whole",
    descriptor[ESPBLE_HID_MOUSE_PADDING_OFFSET] == 5);

  EspBleHidDescriptorSelection vendor;
  vendor.profileMask = 1u << ESPBLE_HID_PROFILE_VENDOR;
  vendor.vendorReportSize = 0x20;
  const size_t vendorLength =
    espBleHidComposeDescriptor(vendor, descriptor, sizeof(descriptor));
  check("vendor descriptor length",
    vendorLength == sizeof(EspBleHidVendorDescriptorTemplate));
  check("vendor sizes patched",
    descriptor[ESPBLE_HID_VENDOR_INPUT_COUNT_OFFSET] == 0x20 &&
    descriptor[ESPBLE_HID_VENDOR_OUTPUT_COUNT_OFFSET] == 0x20 &&
    descriptor[ESPBLE_HID_VENDOR_FEATURE_COUNT_OFFSET] == 0x20);

  // A buffer that cannot hold the selection must be refused, not truncated
  // into a descriptor that parses as something else.
  check("undersized buffer refused",
    espBleHidComposeDescriptor(composite, descriptor, 32) == 0);
  check("null buffer refused",
    espBleHidComposeDescriptor(composite, nullptr, sizeof(descriptor)) == 0);

  // Report packing.
  uint8_t report[64];
  const uint8_t keys[6] = {0x04, 0, 0, 0, 0, 0};
  check("6KRO layout",
    espBleHidPackKeyboardReport(0x02, keys, report, sizeof(report)) == 8 &&
    report[0] == 0x02 && report[1] == 0x00 && report[2] == 0x04);
  const uint8_t bitmap[28] = {0x10};
  check("NKRO layout",
    espBleHidPackNkroKeyboardReport(0x01, bitmap, sizeof(bitmap), report,
      sizeof(report)) == 29 && report[0] == 0x01 && report[1] == 0x10);
  check("mouse layout",
    espBleHidPackMouseReport(0x01, -2, 3, -1, report, sizeof(report)) == 4 &&
    report[0] == 0x01 && report[1] == 0xfe && report[2] == 0x03 &&
    report[3] == 0xff);
  check("consumer usage is little-endian",
    espBleHidPackConsumerReport(0x00e9, report, sizeof(report)) == 2 &&
    report[0] == 0xe9 && report[1] == 0x00);
  check("system layout",
    espBleHidPackSystemReport(0x02, report, sizeof(report)) == 1 &&
    report[0] == 0x02);
  check("packers refuse short buffers",
    espBleHidPackKeyboardReport(0, keys, report, 7) == 0 &&
    espBleHidPackMouseReport(0, 0, 0, 0, report, 3) == 0);

  if (failures) return 1;
  std::puts("OK");
  return 0;
}
