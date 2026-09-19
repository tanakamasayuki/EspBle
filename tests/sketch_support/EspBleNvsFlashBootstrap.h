// Shared sketch body for the fixture bootstrap modules.
//
// EraseFlash=all in sketch.yaml performs the erase before this sketch is
// uploaded. This code then proves that the default NVS partition accepts a
// write and still has free entries. The on-demand '?' response avoids relying
// on a startup banner that may be printed before the serial monitor attaches.
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <nvs.h>

#include "EspBleTestLifecycle.h"

namespace EspBleNvsFlashBootstrap
{
inline bool &healthy()
{
  static bool value = false;
  return value;
}

inline size_t &freeEntries()
{
  static size_t value = 0;
  return value;
}

inline void report()
{
  Serial.printf("FLASH_BOOTSTRAP_READY nvs=%u free=%u\n",
    healthy() ? 1 : 0, static_cast<unsigned>(freeEntries()));
}

inline void begin()
{
  Serial.begin(115200);
  delay(500);

  Preferences preferences;
  const bool opened = preferences.begin("espble_boot", false);
  const uint32_t expected = 0x45535042;
  const bool wrote = opened && preferences.putUInt("probe", expected) == sizeof(expected);
  const bool read = wrote && preferences.getUInt("probe", 0) == expected;
  if (opened)
  {
    preferences.remove("probe");
    preferences.end();
  }

  nvs_stats_t stats{};
  const bool measured = nvs_get_stats(nullptr, &stats) == ESP_OK;
  freeEntries() = measured ? stats.free_entries : 0;
  healthy() = opened && wrote && read && measured && stats.free_entries > 0;
  report();

  EspBleTestLifecycle::begin({});
}

inline void update()
{
  if (Serial.available() > 0)
  {
    const char command = EspBleTestLifecycle::filter(Serial.read());
    if (command == '?') report();
  }
  EspBleTestLifecycle::update();
  delay(10);
}
}
