// The stop action and predicate for a sketch built on one EspBleClassic
// instance. See EspBleTestLifecycle.h for the protocol.
//
//   EspBleTestLifecycle::beginClassic(bluetooth);
//
// STOP calls EspBleClassic::end(), which drops every link and turns the radio
// off. Every later call into the instance fails cleanly.
#pragma once

#include <EspBleClassic.h>

#include "EspBleTestLifecycle.h"

namespace EspBleTestLifecycle
{
namespace detail
{
inline EspBleClassic *&classic()
{
  static EspBleClassic *value = nullptr;
  return value;
}

inline void classicStop()
{
  classic()->end();
}

inline bool classicStopped()
{
  return !classic()->initialized();
}
}

inline void attachClassic(EspBleClassic &bluetooth)
{
  detail::classic() = &bluetooth;
}

inline Hooks classicHooks()
{
  Hooks hooks;
  hooks.stop = detail::classicStop;
  hooks.stopped = detail::classicStopped;
  return hooks;
}

inline void beginClassic(EspBleClassic &bluetooth)
{
  attachClassic(bluetooth);
  begin(classicHooks());
}
}
