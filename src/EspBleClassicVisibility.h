#ifndef ESP_BLE_CLASSIC_VISIBILITY_H
#define ESP_BLE_CLASSIC_VISIBILITY_H

// Internal. One owner for how this device presents itself over Classic: the
// scan mode and the Class of Device.
//
// Every profile used to call esp_bt_gap_set_scan_mode() itself when it started,
// so whichever profile started last decided whether the device answered inquiry
// and accepted connections, and a sketch had no way to say otherwise. Starting
// a profile also rewrites the Class of Device from the services it registers,
// which is why the sketch's value has to be re-applied afterwards rather than
// set once. Both values now live with EspBleClassic; profiles re-assert them
// instead of choosing them.

#include "EspBleClassicBuild.h"

class EspBleClassicVisibilityOwner
{
public:
  // Applies whatever the sketch configured or last asked for — scan mode and,
  // when the sketch named one, the Class of Device. Profiles call this when
  // they start so that starting a profile does not change what the sketch
  // decided.
  static bool apply();
  // A2DP Sink accepts one source at a time and hides the device while that
  // source is connected. It restores the owner's value with apply() afterwards,
  // so an application that chose to stay hidden is not made visible again.
  static bool hideWhileExclusivelyConnected();
};

#endif
