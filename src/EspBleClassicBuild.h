#ifndef ESP_BLE_CLASSIC_BUILD_H
#define ESP_BLE_CLASSIC_BUILD_H

#include <esp_arduino_version.h>
#include <sdkconfig.h>

// The Classic host ships as an archive built against ESP-IDF v5.5.5, and this file
// is compiled for every original-ESP32 sketch -- BLE-only ones included, because
// Arduino builds every .cpp under src/. Cores in the 3.3 line all carry ESP-IDF
// 5.5, where the differences are names rather than layouts and are handled in
// EspBleClassicCoreCompat.h. Arduino-ESP32 3.2 and older carry ESP-IDF 5.4, whose
// Bluetooth callback structures have different members (esp_hf_cb_param_t gained
// preferred_frame_size in 5.5), so the archive would read them at the wrong
// offsets. That is a wrong build rather than a missing name, and the error the
// compiler gives says nothing about it.
#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_BT_CLASSIC_ENABLED)
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 0)
#error "EspBle on the original ESP32 requires Arduino-ESP32 3.3.0 or newer. Cores below it carry ESP-IDF 5.4, whose Bluetooth structures do not match the bundled Classic host. This applies whether or not the sketch uses Bluetooth Classic."
#elif ESP_ARDUINO_VERSION > ESP_ARDUINO_VERSION_VAL(3, 3, 11)
// Newer cores are not blocked, because only their ABI is unverified, not known
// to be wrong. A silent mismatch would be worse than saying so.
#warning "EspBle's Bluetooth Classic host is built for Arduino-ESP32 3.3.11 (ESP-IDF 5.5.5). This core version has not been verified."
#endif
#endif

// The original ESP32 is the only Arduino-ESP32 target with BR/EDR support.
// EspBleClassic always uses EspBle's namespaced, independently built host on
// that target; selecting the supported backend must not require sketch-local
// compiler flags. ESPBLE_CLASSIC_ONLY remains an optional build-size switch.
// Running BLE and Classic together needs no flag either: whether the broker
// routes is decided by which hosts a sketch starts.
#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_BT_CLASSIC_ENABLED)
#ifndef ESPBLE_CLASSIC_CUSTOM_HOST
#define ESPBLE_CLASSIC_CUSTOM_HOST 1
#endif
#ifndef ESPBLE_ENABLE_CLASSIC
#define ESPBLE_ENABLE_CLASSIC 1
#endif
#endif

#endif
