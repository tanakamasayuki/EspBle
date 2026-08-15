#ifndef ESP_BLE_TYPES_H
#define ESP_BLE_TYPES_H

#include <stdint.h>

enum class EspBleError : uint8_t
{
  None = 0,
  InvalidState,
  InvalidArgument,
  BackendFailure,
  ResourceExhausted,
  NotFound,
  Timeout,
};

#endif
