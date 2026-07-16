#ifndef ESP_BLE_KEYMAP_H
#define ESP_BLE_KEYMAP_H

// HID keyboard usage to 8-bit character conversion.
//
// This header is Arduino-independent so the conversion logic and layout
// tables can be verified by host-side unit tests (tests/unit/keymap).
//
// Behavior notes (EspUsbHost-compatible):
// - Only the unshifted and Shift columns exist. AltGr (Right Alt) is not
//   interpreted; characters that require AltGr on a layout are not produced.
// - CapsLock is applied positionally to usages 0x04-0x1d (the QWERTY letter
//   block), not per-character.
// - Non-ASCII characters use ISO-8859-1 (Latin-1). Dead keys map to 0.

#include <stddef.h>
#include <stdint.h>

#include "keymap/keymap_da_dk.h"
#include "keymap/keymap_de_de.h"
#include "keymap/keymap_en_gb.h"
#include "keymap/keymap_es_es.h"
#include "keymap/keymap_fi_fi.h"
#include "keymap/keymap_fr_ch.h"
#include "keymap/keymap_fr_fr.h"
#include "keymap/keymap_hu_hu.h"
#include "keymap/keymap_it_it.h"
#include "keymap/keymap_ja_jp.h"
#include "keymap/keymap_nb_no.h"
#include "keymap/keymap_nl_nl.h"
#include "keymap/keymap_pt_br.h"
#include "keymap/keymap_pt_pt.h"
#include "keymap/keymap_sv_se.h"

// Windows LCID values, matching EspUsbHost layout identifiers.
enum class EspBleKeyboardLayout : uint16_t
{
  ZhTw = 0x0404,
  DaDk = 0x0406,
  DeDe = 0x0407,
  EnUs = 0x0409,
  FiFi = 0x040b,
  FrFr = 0x040c,
  HuHu = 0x040e,
  ItIt = 0x0410,
  JaJp = 0x0411,
  KoKr = 0x0412,
  NlNl = 0x0413,
  NbNo = 0x0414,
  PtBr = 0x0416,
  SvSe = 0x041d,
  ZhCn = 0x0804,
  EnGb = 0x0809,
  PtPt = 0x0816,
  EsEs = 0x0c0a,
  FrCh = 0x100c,
};

inline uint8_t espBleKeypadUsageToAscii(uint8_t usage, bool numLock)
{
  switch (usage)
  {
  case 0x54: return '/';
  case 0x55: return '*';
  case 0x56: return '-';
  case 0x57: return '+';
  case 0x58: return '\r';
  case 0x67: return '=';
  default: break;
  }
  if (!numLock)
  {
    return 0;
  }
  if (usage >= 0x59 && usage <= 0x61)
  {
    return static_cast<uint8_t>('1' + usage - 0x59);
  }
  if (usage == 0x62) return '0';
  if (usage == 0x63) return '.';
  return 0;
}

inline uint8_t espBleUsageToAscii(
  uint8_t usage,
  uint8_t modifiers,
  EspBleKeyboardLayout layout,
  bool capsLock,
  bool numLock)
{
  constexpr uint8_t LeftShiftMask = 0x02;
  constexpr uint8_t RightShiftMask = 0x20;

  if ((usage >= 0x54 && usage <= 0x63) || usage == 0x67)
  {
    return espBleKeypadUsageToAscii(usage, numLock);
  }
  const bool shifted = (modifiers & (LeftShiftMask | RightShiftMask)) != 0;
  const bool effectiveShift =
    usage >= 0x04 && usage <= 0x1d ? shifted != capsLock : shifted;
  const uint8_t (*table)[2] = nullptr;
  size_t tableSize = 128;
  bool useEnUsTable = false;
  switch (layout)
  {
  case EspBleKeyboardLayout::DaDk: table = KEYCODE_TO_ASCII_DA_DK; break;
  case EspBleKeyboardLayout::DeDe: table = KEYCODE_TO_ASCII_DE_DE; break;
  case EspBleKeyboardLayout::EnGb: table = KEYCODE_TO_ASCII_EN_GB; break;
  case EspBleKeyboardLayout::EsEs: table = KEYCODE_TO_ASCII_ES_ES; break;
  case EspBleKeyboardLayout::FiFi: table = KEYCODE_TO_ASCII_FI_FI; break;
  case EspBleKeyboardLayout::FrCh: table = KEYCODE_TO_ASCII_FR_CH; break;
  case EspBleKeyboardLayout::FrFr: table = KEYCODE_TO_ASCII_FR_FR; break;
  case EspBleKeyboardLayout::HuHu: table = KEYCODE_TO_ASCII_HU_HU; break;
  case EspBleKeyboardLayout::ItIt: table = KEYCODE_TO_ASCII_IT_IT; break;
  case EspBleKeyboardLayout::JaJp:
    table = KEYCODE_TO_ASCII_JA_JP;
    tableSize = 0x90;
    break;
  case EspBleKeyboardLayout::NbNo: table = KEYCODE_TO_ASCII_NB_NO; break;
  case EspBleKeyboardLayout::NlNl: table = KEYCODE_TO_ASCII_NL_NL; break;
  case EspBleKeyboardLayout::PtBr:
    table = KEYCODE_TO_ASCII_PT_BR;
    tableSize = 0x90;
    break;
  case EspBleKeyboardLayout::PtPt: table = KEYCODE_TO_ASCII_PT_PT; break;
  case EspBleKeyboardLayout::SvSe: table = KEYCODE_TO_ASCII_SV_SE; break;
  case EspBleKeyboardLayout::EnUs:
  case EspBleKeyboardLayout::KoKr:
  case EspBleKeyboardLayout::ZhCn:
  case EspBleKeyboardLayout::ZhTw:
    useEnUsTable = true;
    break;
  default: return 0;
  }
  if (table != nullptr)
  {
    return usage < tableSize ? table[usage][effectiveShift ? 1 : 0] : 0;
  }
  if (!useEnUsTable)
  {
    return 0;
  }

  if (usage >= 0x04 && usage <= 0x1d)
  {
    const uint8_t letter = static_cast<uint8_t>('a' + usage - 0x04);
    return effectiveShift ? static_cast<uint8_t>(letter - 'a' + 'A') : letter;
  }
  if (usage >= 0x1e && usage <= 0x27)
  {
    static const uint8_t normal[] = "1234567890";
    static const uint8_t shiftedDigits[] = "!@#$%^&*()";
    const size_t index = usage - 0x1e;
    return effectiveShift ? shiftedDigits[index] : normal[index];
  }
  switch (usage)
  {
  case 0x28: return '\r';
  case 0x29: return 0x1b;
  case 0x2a: return '\b';
  case 0x2b: return '\t';
  case 0x2c: return ' ';
  default: break;
  }
  switch (usage)
  {
  case 0x2d: return effectiveShift ? '_' : '-';
  case 0x2e: return effectiveShift ? '+' : '=';
  case 0x2f: return effectiveShift ? '{' : '[';
  case 0x30: return effectiveShift ? '}' : ']';
  case 0x31: return effectiveShift ? '|' : '\\';
  case 0x32: return effectiveShift ? '~' : '#';
  case 0x33: return effectiveShift ? ':' : ';';
  case 0x34: return effectiveShift ? '"' : '\'';
  case 0x35: return effectiveShift ? '~' : '`';
  case 0x36: return effectiveShift ? '<' : ',';
  case 0x37: return effectiveShift ? '>' : '.';
  case 0x38: return effectiveShift ? '?' : '/';
  default: return 0;
  }
}

#endif // ESP_BLE_KEYMAP_H
