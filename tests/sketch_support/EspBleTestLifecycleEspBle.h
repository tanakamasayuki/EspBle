// The stop action and predicate for a sketch built on one EspBle instance.
// See EspBleTestLifecycle.h for the protocol.
//
//   EspBleTestLifecycle::beginEspBle(ble);
//
// STOP calls EspBle::end(), which drops every connection and turns the radio
// off. Every later call into the instance fails cleanly, so loop() may keep
// running as it is.
//
// A sketch that runs EspBle next to EspBleClassic (dual host) calls
// attachEspBle() and attachClassic() and composes espBleHooks() with
// classicHooks() into one Hooks for begin().
#pragma once

#include <EspBle.h>

#include "EspBleTestLifecycle.h"

namespace EspBleTestLifecycle
{
namespace detail
{
inline EspBle *&espBle()
{
  static EspBle *value = nullptr;
  return value;
}

inline void espBleStop()
{
  espBle()->end();
}

inline bool espBleStopped()
{
  return !espBle()->initialized();
}
}

// Remembers the instance; begin() is separate so that a dual-host sketch can
// compose these hooks with the Classic ones.
inline void attachEspBle(EspBle &ble)
{
  detail::espBle() = &ble;
}

inline Hooks espBleHooks()
{
  Hooks hooks;
  hooks.stop = detail::espBleStop;
  hooks.stopped = detail::espBleStopped;
  return hooks;
}

inline void beginEspBle(EspBle &ble)
{
  attachEspBle(ble);
  begin(espBleHooks());
}
}
