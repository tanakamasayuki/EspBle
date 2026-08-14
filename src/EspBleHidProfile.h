#ifndef ESP_BLE_HID_PROFILE_H
#define ESP_BLE_HID_PROFILE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// The HID Report Descriptors of the built-in profiles, and the rules for
// composing them into one descriptor.
//
// HID over GATT and HID over BR/EDR differ in how the descriptor and the
// reports reach the peer, not in what they contain: the same descriptor bytes
// and the same report layouts work on both. Keeping them here lets the BLE and
// the Classic implementations produce identical bytes, so a device behaves the
// same way over either transport and a host decodes it with one parser.
//
// This header has no Arduino and no backend dependency, so the composition can
// be checked with host unit tests.

// Report IDs are part of the descriptor and therefore part of the wire format.
// The built-in profiles reserve 1 to 6; a custom descriptor has to avoid them.
enum : uint8_t
{
  ESPBLE_HID_REPORT_ID_KEYBOARD = 1,
  ESPBLE_HID_REPORT_ID_MOUSE = 2,
  ESPBLE_HID_REPORT_ID_GAMEPAD = 3,
  ESPBLE_HID_REPORT_ID_CONSUMER = 4,
  ESPBLE_HID_REPORT_ID_SYSTEM = 5,
  ESPBLE_HID_REPORT_ID_VENDOR = 6,
};

// Bit positions in the profile mask, in the order the descriptors are
// concatenated. The order is part of the wire format too: a host that already
// paired with this device parses the descriptor it stored.
enum : uint8_t
{
  ESPBLE_HID_PROFILE_KEYBOARD = 0,
  ESPBLE_HID_PROFILE_MOUSE = 1,
  ESPBLE_HID_PROFILE_GAMEPAD = 2,
  ESPBLE_HID_PROFILE_CONSUMER = 3,
  ESPBLE_HID_PROFILE_SYSTEM = 4,
  ESPBLE_HID_PROFILE_VENDOR = 5,
  ESPBLE_HID_PROFILE_COUNT = 6,
};

// 6KRO: modifiers, one constant byte, then six key slots. This is also the
// layout the Boot Protocol requires, which is why the slots are not a bitmap.
static const uint8_t EspBleHidKeyboardDescriptor[] = {
  0x05, 0x01,       // Usage Page (Generic Desktop)
  0x09, 0x06,       // Usage (Keyboard)
  0xa1, 0x01,       // Collection (Application)
  0x85, 0x01,       // Report ID
  0x05, 0x07,       // Usage Page (Keyboard)
  0x19, 0xe0,       // Usage Minimum (Left Control)
  0x29, 0xe7,       // Usage Maximum (Right GUI)
  0x15, 0x00,       // Logical Minimum (0)
  0x25, 0x01,       // Logical Maximum (1)
  0x75, 0x01,       // Report Size (1)
  0x95, 0x08,       // Report Count (8)
  0x81, 0x02,       // Input (Data, Variable, Absolute)
  0x95, 0x01,       // Report Count (1)
  0x75, 0x08,       // Report Size (8)
  0x81, 0x01,       // Input (Constant)
  0x95, 0x06,       // Report Count (6)
  0x75, 0x08,       // Report Size (8)
  0x15, 0x00,       // Logical Minimum (0)
  0x25, 0x65,       // Logical Maximum (101)
  0x05, 0x07,       // Usage Page (Keyboard)
  0x19, 0x00,       // Usage Minimum (0)
  0x29, 0x65,       // Usage Maximum (101)
  0x81, 0x00,       // Input (Data, Array)
  0x95, 0x05,       // Report Count (5)
  0x75, 0x01,       // Report Size (1)
  0x05, 0x08,       // Usage Page (LEDs)
  0x19, 0x01,       // Usage Minimum (Num Lock)
  0x29, 0x05,       // Usage Maximum (Kana)
  0x91, 0x02,       // Output (Data, Variable, Absolute)
  0x95, 0x01,       // Report Count (1)
  0x75, 0x03,       // Report Size (3)
  0x91, 0x01,       // Output (Constant)
  0xc0              // End Collection
};

// NKRO: the six slots become a 224-bit usage bitmap, so every key can be held
// at once. The LED output report keeps the same shape as the 6KRO one.
static const uint8_t EspBleHidNkroKeyboardDescriptor[] = {
  0x05,0x01, 0x09,0x06, 0xa1,0x01, 0x85,0x01,
  0x05,0x07, 0x19,0xe0, 0x29,0xe7, 0x15,0x00, 0x25,0x01,
  0x75,0x01, 0x95,0x08, 0x81,0x02,
  0x05,0x08, 0x19,0x01, 0x29,0x05, 0x95,0x05, 0x75,0x01,
  0x91,0x02, 0x95,0x01, 0x75,0x03, 0x91,0x01,
  0x05,0x07, 0x19,0x00, 0x29,0xdf, 0x15,0x00, 0x25,0x01,
  0x75,0x01, 0x95,0xe0, 0x81,0x02, 0xc0
};

static const uint8_t EspBleHidMouseDescriptor[] = {
  0x05,0x01, 0x09,0x02, 0xa1,0x01, 0x85,0x02, 0x09,0x01, 0xa1,0x00,
  0x05,0x09, 0x19,0x01, 0x29,0x05, 0x15,0x00, 0x25,0x01, 0x95,0x05,
  0x75,0x01, 0x81,0x02, 0x95,0x01, 0x75,0x03, 0x81,0x01, 0x05,0x01,
  0x09,0x30, 0x09,0x31, 0x09,0x38, 0x15,0x81, 0x25,0x7f, 0x75,0x08,
  0x95,0x03, 0x81,0x06, 0xc0, 0xc0};

static const uint8_t EspBleHidGamepadDescriptor[] = {
  0x05,0x01, 0x09,0x05, 0xa1,0x01, 0x85,0x03, 0x15,0x81, 0x25,0x7f,
  0x09,0x30, 0x09,0x31, 0x09,0x32, 0x09,0x35, 0x09,0x33, 0x09,0x34,
  0x75,0x08, 0x95,0x06, 0x81,0x02, 0x15,0x00, 0x25,0x08, 0x35,0x00,
  0x46,0x3b,0x01, 0x65,0x14, 0x09,0x39, 0x75,0x08, 0x95,0x01,
  0x81,0x02, 0x65,0x00, 0x05,0x09, 0x19,0x01, 0x29,0x20, 0x15,0x00,
  0x25,0x01, 0x75,0x01, 0x95,0x20, 0x81,0x02, 0xc0};

static const uint8_t EspBleHidConsumerDescriptor[] = {
  0x05,0x0c, 0x09,0x01, 0xa1,0x01, 0x85,0x04, 0x15,0x00, 0x26,0xff,0x03,
  0x19,0x00, 0x2a,0xff,0x03, 0x75,0x10, 0x95,0x01, 0x81,0x00, 0xc0};

static const uint8_t EspBleHidSystemDescriptor[] = {
  0x05,0x01, 0x09,0x80, 0xa1,0x01, 0x85,0x05, 0x15,0x00, 0x25,0x03,
  0x19,0x00, 0x29,0x03, 0x75,0x08, 0x95,0x01, 0x81,0x00, 0xc0};

// The vendor report size is chosen by the sketch, so the descriptor is built
// rather than stored. Offsets 19, 25 and 31 are the input, output and feature
// report counts.
static const uint8_t EspBleHidVendorDescriptorTemplate[] = {
  0x06,0x00,0xff, 0x09,0x01, 0xa1,0x01, 0x85,0x06,
  0x15,0x00, 0x26,0xff,0x00, 0x75,0x08,
  0x09,0x01, 0x95,0x3f, 0x81,0x02,
  0x09,0x02, 0x95,0x3f, 0x91,0x02,
  0x09,0x03, 0x95,0x3f, 0xb1,0x02, 0xc0};

// Offsets patched inside a copied mouse descriptor when the sketch asks for a
// different number of buttons: the usage maximum, the report count, and the
// padding that keeps the byte whole.
enum : size_t
{
  ESPBLE_HID_MOUSE_USAGE_MAXIMUM_OFFSET = 17,
  ESPBLE_HID_MOUSE_BUTTON_COUNT_OFFSET = 23,
  ESPBLE_HID_MOUSE_PADDING_OFFSET = 31,
};

enum : size_t
{
  ESPBLE_HID_VENDOR_INPUT_COUNT_OFFSET = 19,
  ESPBLE_HID_VENDOR_OUTPUT_COUNT_OFFSET = 25,
  ESPBLE_HID_VENDOR_FEATURE_COUNT_OFFSET = 31,
};

struct EspBleHidDescriptorSelection
{
  // Bit per ESPBLE_HID_PROFILE_*.
  uint8_t profileMask = 0;
  bool keyboardNkro = false;
  uint8_t mouseButtonCount = 5;
  uint8_t vendorReportSize = 0x3f;
};

// Concatenates the selected profile descriptors in wire order and applies the
// per-profile patches. Returns the number of bytes written, or 0 when the
// output is too small.
inline size_t espBleHidComposeDescriptor(
  const EspBleHidDescriptorSelection &selection, uint8_t *output,
  size_t capacity)
{
  if (output == nullptr) return 0;

  uint8_t vendor[sizeof(EspBleHidVendorDescriptorTemplate)];
  memcpy(vendor, EspBleHidVendorDescriptorTemplate, sizeof(vendor));
  vendor[ESPBLE_HID_VENDOR_INPUT_COUNT_OFFSET] = selection.vendorReportSize;
  vendor[ESPBLE_HID_VENDOR_OUTPUT_COUNT_OFFSET] = selection.vendorReportSize;
  vendor[ESPBLE_HID_VENDOR_FEATURE_COUNT_OFFSET] = selection.vendorReportSize;

  struct Part { const uint8_t *data; size_t length; };
  const Part keyboard = selection.keyboardNkro
    ? Part{EspBleHidNkroKeyboardDescriptor,
           sizeof(EspBleHidNkroKeyboardDescriptor)}
    : Part{EspBleHidKeyboardDescriptor, sizeof(EspBleHidKeyboardDescriptor)};
  const Part parts[ESPBLE_HID_PROFILE_COUNT] = {
    keyboard,
    {EspBleHidMouseDescriptor, sizeof(EspBleHidMouseDescriptor)},
    {EspBleHidGamepadDescriptor, sizeof(EspBleHidGamepadDescriptor)},
    {EspBleHidConsumerDescriptor, sizeof(EspBleHidConsumerDescriptor)},
    {EspBleHidSystemDescriptor, sizeof(EspBleHidSystemDescriptor)},
    {vendor, sizeof(vendor)},
  };

  size_t used = 0;
  for (uint8_t index = 0; index < ESPBLE_HID_PROFILE_COUNT; ++index)
  {
    if ((selection.profileMask & static_cast<uint8_t>(1u << index)) == 0)
      continue;
    if (used + parts[index].length > capacity) return 0;
    const size_t offset = used;
    memcpy(output + used, parts[index].data, parts[index].length);
    used += parts[index].length;
    if (index == ESPBLE_HID_PROFILE_MOUSE)
    {
      output[offset + ESPBLE_HID_MOUSE_USAGE_MAXIMUM_OFFSET] =
        selection.mouseButtonCount;
      output[offset + ESPBLE_HID_MOUSE_BUTTON_COUNT_OFFSET] =
        selection.mouseButtonCount;
      output[offset + ESPBLE_HID_MOUSE_PADDING_OFFSET] =
        static_cast<uint8_t>(8 - selection.mouseButtonCount);
    }
  }
  return used;
}

// Button and hat constants, and the report types the sketch fills in. They
// live here rather than in a transport header so the same report can be sent
// over BLE or over Classic without translating between two shapes.

static constexpr uint8_t ESP_BLE_HID_MOUSE_LEFT = 0x01;
static constexpr uint8_t ESP_BLE_HID_MOUSE_RIGHT = 0x02;
static constexpr uint8_t ESP_BLE_HID_MOUSE_MIDDLE = 0x04;
static constexpr uint8_t ESP_BLE_HID_MOUSE_BACK = 0x08;
static constexpr uint8_t ESP_BLE_HID_MOUSE_FORWARD = 0x10;

static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_CENTER = 0x00;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_UP = 0x01;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_UP_RIGHT = 0x02;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_RIGHT = 0x03;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_DOWN_RIGHT = 0x04;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_DOWN = 0x05;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_DOWN_LEFT = 0x06;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_LEFT = 0x07;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_UP_LEFT = 0x08;



struct EspBleHidKeyboardInputReport
{
  static constexpr uint8_t LeftControl = 0x01;
  static constexpr uint8_t LeftShift = 0x02;
  static constexpr uint8_t LeftAlt = 0x04;
  static constexpr uint8_t LeftGui = 0x08;
  static constexpr uint8_t RightControl = 0x10;
  static constexpr uint8_t RightShift = 0x20;
  static constexpr uint8_t RightAlt = 0x40;
  static constexpr uint8_t RightGui = 0x80;

  uint8_t modifiers = 0;
  uint8_t keys[6] = {};
};

// Full NKRO keyboard state in one report: modifier byte + a bitmap of usages
// 0x00-0xDF (the EspUsbDevice-compatible 29-byte layout). Modifier usages
// 0xE0-0xE7 live in `modifiers`, not the bitmap, and press() / release() route
// them there automatically. isDown() matches the Host-side
// EspBleHidKeyboardState accessor so a Host snapshot can be replayed on a
// Device with the same vocabulary; the bitmaps differ in size (the Host tracks
// usages up to 0xFF, this report up to MaxBitmapUsage).
struct EspBleHidKeyboardNkroReport
{
  static constexpr size_t BitmapSize = 28;
  static constexpr uint8_t MaxBitmapUsage = 0xdf;

  uint8_t modifiers = 0;
  // A bitmap, not a usage array (see EspBleHidKeyboardState::bitmap).
  uint8_t bitmap[BitmapSize] = {};

  void clear()
  {
    modifiers = 0;
    for (size_t index = 0; index < BitmapSize; ++index) bitmap[index] = 0;
  }

  // Returns false when the usage is above MaxBitmapUsage and is not a modifier
  // (0xE0-0xE7), i.e. this report cannot represent it.
  bool press(uint8_t usage)
  {
    if (usage >= 0xe0 && usage <= 0xe7)
    {
      modifiers |= static_cast<uint8_t>(1u << (usage - 0xe0));
      return true;
    }
    if (usage > MaxBitmapUsage) return false;
    bitmap[usage >> 3] |= static_cast<uint8_t>(1u << (usage & 7));
    return true;
  }

  bool release(uint8_t usage)
  {
    if (usage >= 0xe0 && usage <= 0xe7)
    {
      modifiers &= static_cast<uint8_t>(~(1u << (usage - 0xe0)));
      return true;
    }
    if (usage > MaxBitmapUsage) return false;
    bitmap[usage >> 3] &= static_cast<uint8_t>(~(1u << (usage & 7)));
    return true;
  }

  bool isDown(uint8_t usage) const
  {
    if (usage >= 0xe0 && usage <= 0xe7)
    {
      return (modifiers & static_cast<uint8_t>(1u << (usage - 0xe0))) != 0;
    }
    if (usage > MaxBitmapUsage) return false;
    return (bitmap[usage >> 3] & static_cast<uint8_t>(1u << (usage & 7))) != 0;
  }
};

struct EspBleHidMouseReport
{
  uint8_t buttons = 0;
  int8_t x = 0;
  int8_t y = 0;
  int8_t wheel = 0;
};

struct EspBleHidGamepadReport
{
  int8_t x = 0;
  int8_t y = 0;
  int8_t z = 0;
  int8_t rz = 0;
  int8_t rx = 0;
  int8_t ry = 0;
  uint8_t hat = ESP_BLE_HID_GAMEPAD_HAT_CENTER;
  uint32_t buttons = 0;
};

using EspBleHidKeyboardReport = EspBleHidKeyboardInputReport;

// Report packing. These are the payloads that follow the report ID, in the
// layout the descriptors above declare.

// 6KRO keyboard: modifiers, constant, six usages.
inline size_t espBleHidPackKeyboardReport(
  uint8_t modifiers, const uint8_t keys[6], uint8_t *output, size_t capacity)
{
  if (output == nullptr || capacity < 8) return 0;
  output[0] = modifiers;
  output[1] = 0;
  memcpy(output + 2, keys, 6);
  return 8;
}

// NKRO keyboard: modifiers, then a bit per usage from 0 to 223.
inline size_t espBleHidPackNkroKeyboardReport(
  uint8_t modifiers, const uint8_t *bitmap, size_t bitmapLength,
  uint8_t *output, size_t capacity)
{
  if (output == nullptr || bitmap == nullptr || capacity < 1 + bitmapLength)
    return 0;
  output[0] = modifiers;
  memcpy(output + 1, bitmap, bitmapLength);
  return 1 + bitmapLength;
}

inline size_t espBleHidPackMouseReport(
  uint8_t buttons, int8_t x, int8_t y, int8_t wheel, uint8_t *output,
  size_t capacity)
{
  if (output == nullptr || capacity < 4) return 0;
  output[0] = buttons;
  output[1] = static_cast<uint8_t>(x);
  output[2] = static_cast<uint8_t>(y);
  output[3] = static_cast<uint8_t>(wheel);
  return 4;
}

// Consumer Control carries a 16-bit usage, little-endian.
inline size_t espBleHidPackConsumerReport(
  uint16_t usage, uint8_t *output, size_t capacity)
{
  if (output == nullptr || capacity < 2) return 0;
  output[0] = static_cast<uint8_t>(usage & 0xff);
  output[1] = static_cast<uint8_t>(usage >> 8);
  return 2;
}

inline size_t espBleHidPackSystemReport(
  uint8_t usage, uint8_t *output, size_t capacity)
{
  if (output == nullptr || capacity < 1) return 0;
  output[0] = usage;
  return 1;
}

#endif
