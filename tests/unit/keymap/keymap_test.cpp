// Host-side unit tests for the HID usage to 8-bit character conversion.
// Expected values follow each layout's national standard (Windows layout
// data; non-ASCII in ISO-8859-1, dead keys produce 0).

#include "EspBleKeymap.h"

#include <cstdio>

namespace
{
int failures = 0;
constexpr uint8_t Shift = 0x02; // Left Shift modifier bit
constexpr uint8_t AltGr = 0x40; // Right Alt (AltGr) modifier bit

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

uint16_t convU(
  uint8_t usage,
  uint8_t modifiers,
  EspBleKeyboardLayout layout,
  bool capsLock = false,
  bool numLock = false)
{
  return espBleUsageToUnicode(usage, modifiers, layout, capsLock, numLock);
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

  // AltGr layer (Windows KBDGR / DIN 2137 T1), EspUsbHost-compatible.
  check("deDE AltGr q (@)", conv(0x14, AltGr, DeDe), '@');
  check("deDE AltGr m (micro)", conv(0x10, AltGr, DeDe), 0xb5);
  check("deDE AltGr 7 ({)", conv(0x24, AltGr, DeDe), '{');
  check("deDE AltGr ss (backslash)", conv(0x2d, AltGr, DeDe), '\\');
  check("deDE AltGr < (pipe)", conv(0x64, AltGr, DeDe), '|');
  // AltGr held on a key with no AltGr value falls back to base/Shift.
  check("deDE AltGr a falls back to a", conv(0x04, AltGr, DeDe), 'a');
  check("deDE AltGr shift a falls back to A", conv(0x04, AltGr | Shift, DeDe), 'A');
  // AltGr wins over Shift on a key that has an AltGr value.
  check("deDE AltGr+shift q still @", conv(0x14, AltGr | Shift, DeDe), '@');
  // Non-Latin-1 characters appear in unicode; ascii reports 0 for them.
  check("deDE AltGr e unicode EUR", convU(0x08, AltGr, DeDe), 0x20ac);
  check("deDE AltGr e ascii 0 (EUR not Latin-1)", conv(0x08, AltGr, DeDe), 0);
  // it-IT: AltGr+Shift selects the level-4 column when present.
  check("itIT AltGr e-grave ([)",
        conv(0x2f, AltGr, EspBleKeyboardLayout::ItIt), '[');
  check("itIT AltGr+Shift e-grave ({)",
        conv(0x2f, AltGr | Shift, EspBleKeyboardLayout::ItIt), '{');

  // fr-FR (AZERTY).
  check("frFR a position", conv(0x04, 0, FrFr), 'q');
  check("frFR ; position", conv(0x33, 0, FrFr), 'm');
  check("frFR AltGr 0 (@)", conv(0x27, AltGr, FrFr), '@');
  check("frFR AltGr e unicode EUR", convU(0x08, AltGr, FrFr), 0x20ac);

  // CapsLock applies only to real cased-letter keys (Shift column is the
  // uppercase of the unshifted column), EspUsbHost-compatible.
  check("enUS capslock 1 stays 1", conv(0x1e, 0, EnUs, true), '1');
  check("deDE capslock u-umlaut -> U-umlaut", conv(0x2f, 0, DeDe, true), 0xdc);
  check("deDE capslock+shift u-umlaut -> u-umlaut", conv(0x2f, Shift, DeDe, true), 0xfc);
  check("deDE capslock eszett stays eszett", conv(0x2d, 0, DeDe, true), 0xdf);
  check("frFR capslock m -> M", conv(0x33, 0, FrFr, true), 'M');
  check("frFR capslock comma-key stays ','", conv(0x10, 0, FrFr, true), ',');
  // CapsLock does not disturb the AltGr layer.
  check("deDE capslock + AltGr q still @", conv(0x14, AltGr, DeDe, true), '@');

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
