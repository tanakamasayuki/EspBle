#ifndef ESP_BLE_HCI_CONTROLLER_POLICY_H
#define ESP_BLE_HCI_CONTROLLER_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESPBLE_HCI_CONTROLLER_POLICY_HOST_COUNT 2
#define ESPBLE_HCI_CONTROLLER_POLICY_MASK_COUNT 3

typedef enum
{
  ESPBLE_HCI_CONTROLLER_POLICY_PASSTHROUGH = 0,
  ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN,
  ESPBLE_HCI_CONTROLLER_POLICY_INVALID_PACKET,
} espble_hci_controller_policy_result_t;

typedef struct
{
  uint8_t masks[ESPBLE_HCI_CONTROLLER_POLICY_MASK_COUNT]
    [ESPBLE_HCI_CONTROLLER_POLICY_HOST_COUNT][8];
  bool present[ESPBLE_HCI_CONTROLLER_POLICY_MASK_COUNT]
    [ESPBLE_HCI_CONTROLLER_POLICY_HOST_COUNT];
} espble_hci_controller_policy_t;

void espble_hci_controller_policy_init(
  espble_hci_controller_policy_t *policy);
void espble_hci_controller_policy_remove_host(
  espble_hci_controller_policy_t *policy, uint8_t host);
bool espble_hci_controller_policy_is_reset(
  const uint8_t *packet, size_t length);

// Caches each host's Set Event Mask request and writes the union required by
// every registered logical host. General, Page 2, and LE masks are independent.
// Non-mask H4 commands are left untouched and return PASSTHROUGH.
espble_hci_controller_policy_result_t
espble_hci_controller_policy_rewrite_command(
  espble_hci_controller_policy_t *policy, uint8_t host,
  const uint8_t *packet, size_t length, uint8_t *output, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
