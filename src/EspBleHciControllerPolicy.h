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

typedef enum
{
  ESPBLE_HCI_CONTROLLER_POLICY_PHYSICAL = 0,
  ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_COMPLETE,
  ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_NO_RESPONSE,
  ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET,
} espble_hci_controller_policy_virtual_action_t;

// Every command observed from the two hosts is assigned a scope before it is
// allowed onto a shared controller. Unknown commands fail closed only while
// dual-host mode is active; single-host pass-through remains unchanged.
typedef enum
{
  ESPBLE_HCI_COMMAND_SCOPE_UNKNOWN = 0,
  ESPBLE_HCI_COMMAND_SCOPE_SHARED_READ,
  ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_RADIO,
  ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_RADIO,
  ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_CONNECTION,
  ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_CONNECTION,
  ESPBLE_HCI_COMMAND_SCOPE_SHARED_CONNECTION,
  ESPBLE_HCI_COMMAND_SCOPE_CONTROLLER_MERGED,
  ESPBLE_HCI_COMMAND_SCOPE_CONTROLLER_VIRTUAL,
  ESPBLE_HCI_COMMAND_SCOPE_HOST_CREDIT,
} espble_hci_command_scope_t;

typedef enum
{
  ESPBLE_HCI_COMMAND_AUTHORIZED = 0,
  ESPBLE_HCI_COMMAND_INVALID_PACKET,
  ESPBLE_HCI_COMMAND_UNCLASSIFIED,
  ESPBLE_HCI_COMMAND_WRONG_HOST,
} espble_hci_command_authorization_t;

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
espble_hci_controller_policy_virtual_action_t
espble_hci_controller_policy_virtual_action(
  const uint8_t *packet, size_t length);
espble_hci_command_scope_t espble_hci_controller_policy_classify_opcode(
  uint16_t opcode);
// True only when command parameter byte 0 is a connection handle whose owner
// must match the sending logical host. Connection-scoped commands addressed by
// Bluetooth device address (for example Accept Synchronous Connection Request)
// remain host-restricted by their scope but do not use handle ownership.
bool espble_hci_controller_policy_targets_handle(uint16_t opcode);
espble_hci_command_authorization_t espble_hci_controller_policy_authorize(
  uint8_t host, const uint8_t *packet, size_t length);

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
