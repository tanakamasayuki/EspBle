// Host-side unit tests for the HID usage to 8-bit character conversion.
// Expected values follow each layout's national standard (Windows layout
// data; non-ASCII in ISO-8859-1, dead keys produce 0).

#include "EspBleKeymap.h"

#include <cstdio>

namespace
{
int failures = 0;
constexpr uint8_t Shift = 0x02;      // Left Shift modifier bit
constexpr uint8_t RightAltMod = 0x40; // Right Alt (AltGr) modifier bit

void check(const char *name, unsigned actual, unsigned expected)
{
  if (actual != expected)
  {
    std::printf("FAIL %-40s got 0x%02x expected 0x%02x\n", name, actual, expected);
    ++failures;
  }
}

uint8_t conv(
  uint8_t usage,
  uint8_t modifiers,
  EspBleKeyboardLayout layout,
  bool capsLock = false,
  bool numLock = false)
{
  return espBleUsageToAscii(usage, modifiers, layout, capsLock, numLock);
}
} // namespace

int main()
{
  const auto EnUs = EspBleKeyboardLayout::EnUs;
  const auto EnGb = EspBleKeyboardLayout::EnGb;
  const auto DeDe = EspBleKeyboardLayout::DeDe;
  const auto FrFr = EspBleKeyboardLayout::FrFr;
  const auto JaJp = EspBleKeyboardLayout::JaJp;
  const auto NlNl = EspBleKeyboardLayout::NlNl;
  const auto PtBr = EspBleKeyboardLayout::PtBr;

  // en-US basics.
  check("enUS a", conv(0x04, 0, EnUs), 'a');
  check("enUS shift a", conv(0x04, Shift, EnUs), 'A');
  check("enUS capslock a", conv(0x04, 0, EnUs, true), 'A');
  check("enUS capslock+shift a", conv(0x04, Shift, EnUs, true), 'a');
  check("enUS shift 2", conv(0x1f, Shift, EnUs), '@');
  check("enUS enter", conv(0x28, 0, EnUs), '\r');
  check("enUS out of range", conv(0x87, 0, EnUs), 0);

  // Keypad handling is layout-independent.
  check("keypad / (numlock off)", conv(0x54, 0, EnUs, false, false), '/');
  check("keypad 1 (numlock on)", conv(0x59, 0, EnUs, false, true), '1');
  check("keypad 1 (numlock off)", conv(0x59, 0, EnUs, false, false), 0);

  // en-GB.
  check("enGB shift 2", conv(0x1f, Shift, EnGb), '"');
  check("enGB shift 3", conv(0x20, Shift, EnGb), 0xa3); // pound sign

  // de-DE (QWERTZ).
  check("deDE y position", conv(0x1c, 0, DeDe), 'z');
  check("deDE z position", conv(0x1d, 0, DeDe), 'y');
  check("deDE eszett", conv(0x2d, 0, DeDe), 0xdf);
  // AltGr is intentionally not interpreted (EspUsbHost-compatible).
  check("deDE altgr q (compat)", conv(0x14, RightAltMod, DeDe), 'q');

  // fr-FR (AZERTY).
  check("frFR a position", conv(0x04, 0, FrFr), 'q');
  check("frFR ; position", conv(0x33, 0, FrFr), 'm');
  // CapsLock applies positionally to 0x04-0x1d only (EspUsbHost-compatible).
  check("frFR capslock m position (compat)", conv(0x33, 0, FrFr, true), 'm');

  // ja-JP (JIS).
  check("jaJP shift 2", conv(0x1f, Shift, JaJp), '"');
  check("jaJP ro", conv(0x87, 0, JaJp), '\\');
  check("jaJP shift ro", conv(0x87, Shift, JaJp), '_');
  check("jaJP yen", conv(0x89, 0, JaJp), '\\');
  check("jaJP shift yen", conv(0x89, Shift, JaJp), '|');

  // nl-NL (Dutch, Windows KBDNE). Digits row shift characters.
  check("nlNL shift 2", conv(0x1f, Shift, NlNl), '"');
  check("nlNL shift 6", conv(0x23, Shift, NlNl), '&');
  check("nlNL shift 7", conv(0x24, Shift, NlNl), '_');
  check("nlNL shift 8", conv(0x25, Shift, NlNl), '(');
  check("nlNL shift 9", conv(0x26, Shift, NlNl), ')');
  check("nlNL shift 0", conv(0x27, Shift, NlNl), '\'');
  // Symbol keys.
  check("nlNL minus position", conv(0x2d, 0, NlNl), '/');
  check("nlNL shift minus position", conv(0x2d, Shift, NlNl), '?');
  check("nlNL equals position", conv(0x2e, 0, NlNl), 0xb0); // degree
  check("nlNL shift equals position (dead ~)", conv(0x2e, Shift, NlNl), 0);
  check("nlNL [ position (dead trema)", conv(0x2f, 0, NlNl), 0);
  check("nlNL ] position", conv(0x30, 0, NlNl), '*');
  check("nlNL shift ] position", conv(0x30, Shift, NlNl), '|');
  check("nlNL NUHS", conv(0x32, 0, NlNl), '<');
  check("nlNL shift NUHS", conv(0x32, Shift, NlNl), '>');
  check("nlNL ; position", conv(0x33, 0, NlNl), '+');
  check("nlNL shift ; position", conv(0x33, Shift, NlNl), 0xb1); // plus-minus
  check("nlNL ' position (dead acute)", conv(0x34, 0, NlNl), 0);
  check("nlNL grave position", conv(0x35, 0, NlNl), '@');
  check("nlNL shift grave position", conv(0x35, Shift, NlNl), 0xa7); // section
  check("nlNL shift comma", conv(0x36, Shift, NlNl), ';');
  check("nlNL shift period", conv(0x37, Shift, NlNl), ':');
  check("nlNL slash position", conv(0x38, 0, NlNl), '-');
  check("nlNL shift slash position", conv(0x38, Shift, NlNl), '=');
  check("nlNL NUBS", conv(0x64, 0, NlNl), ']');
  check("nlNL shift NUBS", conv(0x64, Shift, NlNl), '[');

  // pt-BR (ABNT2).
  check("ptBR cedilla", conv(0x33, 0, PtBr), 0xe7);
  check("ptBR slash key stays ;", conv(0x38, 0, PtBr), ';');
  check("ptBR International1 /", conv(0x87, 0, PtBr), '/');
  check("ptBR shift International1 ?", conv(0x87, Shift, PtBr), '?');
  check("ptBR keypad comma", conv(0x85, 0, PtBr), ',');

  if (failures != 0)
  {
    std::printf("%d check(s) failed\n", failures);
    return 1;
  }
  std::puts("OK");
  return 0;
}
