#ifndef ESP_BLE_CLASSIC_BUILD_H
#define ESP_BLE_CLASSIC_BUILD_H

#include <sdkconfig.h>

// The original ESP32 is the only Arduino-ESP32 target with BR/EDR support.
// EspBleClassic always uses EspBle's namespaced, independently built host on
// that target; selecting the supported backend must not require sketch-local
// compiler flags. ESPBLE_CLASSIC_ONLY remains an optional build-size switch,
// while dual-host still requires its explicit experimental opt-in.
#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_BT_CLASSIC_ENABLED)
#ifndef ESPBLE_CLASSIC_CUSTOM_HOST
#define ESPBLE_CLASSIC_CUSTOM_HOST 1
#endif
#ifndef ESPBLE_ENABLE_CLASSIC
#define ESPBLE_ENABLE_CLASSIC 1
#endif
#endif

#endif
