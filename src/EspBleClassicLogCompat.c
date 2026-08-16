// The Classic host archive is built against ESP-IDF v5.5.5, whose ESP_LOGx
// macros compile into calls to esp_log() -- the log-v2 entry point that first
// appeared in IDF 5.5. Arduino-ESP32 cores below 3.3.0 carry IDF 5.4, which
// has no such symbol, and it is the archive's only import those cores cannot
// satisfy. This file provides it there, forwarding to the log-v1 API the old
// cores do have, with the same "L (time) tag:" framing their own macros used.
//
// On 3.3.0 and newer the core defines esp_log itself and this file compiles to
// nothing, so the real implementation -- per-tag filtering, binary mode and
// all -- is never shadowed on a core that has it.
#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_BT_CLASSIC_ENABLED)

#include <esp_arduino_version.h>

#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 0)

#include <esp_log.h>
#include <stdarg.h>
#include <stdint.h>

// The first parameter is esp_log_config_t: a 32-bit by-value struct whose low
// ESP_LOG_LEVEL_LEN (3) bits are the esp_log_level_t. Taking it as uint32_t
// keeps this file free of the v5.5-only header while matching the call ABI.
void esp_log(uint32_t config, const char *tag, const char *format, ...)
{
  static const char letters[] = {'N', 'E', 'W', 'I', 'D', 'V'};
  const esp_log_level_t level = (esp_log_level_t)(config & 0x7u);
  const char letter = level < sizeof(letters) ? letters[level] : '?';
  esp_log_write(level, tag, "%c (%lu) %s: ", letter,
    (unsigned long)esp_log_timestamp(), tag);
  va_list args;
  va_start(args, format);
  esp_log_writev(level, tag, format, args);
  va_end(args);
  esp_log_write(level, tag, "\n");
}

#endif
#endif
