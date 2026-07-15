#ifndef ESP_BLE_H
#define ESP_BLE_H

#include <sdkconfig.h>

#if !defined(CONFIG_NIMBLE_ENABLED) && !defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"
#endif

#include "espble_version.h"

// The public API is intentionally not defined while docs/API_DESIGN.ja.md is
// being finalized. This header establishes the Arduino library include name
// without committing to speculative classes or ownership rules.

#endif // ESP_BLE_H
