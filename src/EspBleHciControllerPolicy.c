#include "EspBleHciControllerPolicy.h"

#include <string.h>

enum
{
  H4_COMMAND = 0x01,
  OPCODE_SET_EVENT_MASK = 0x0c01,
  OPCODE_RESET = 0x0c03,
  OPCODE_SET_EVENT_MASK_PAGE_2 = 0x0c63,
  OPCODE_LE_SET_EVENT_MASK = 0x2001,
  EVENT_MASK_PARAMETER_LENGTH = 8,
  EVENT_MASK_COMMAND_LENGTH = 12,
};

bool espble_hci_controller_policy_is_reset(
  const uint8_t *packet, size_t length)
{
  return packet != NULL && length == 4 && packet[0] == H4_COMMAND &&
    packet[1] == (OPCODE_RESET & 0xff) &&
    packet[2] == (OPCODE_RESET >> 8) && packet[3] == 0;
}

static int mask_index(uint16_t opcode)
{
  switch (opcode)
  {
    case OPCODE_SET_EVENT_MASK: return 0;
    case OPCODE_SET_EVENT_MASK_PAGE_2: return 1;
    case OPCODE_LE_SET_EVENT_MASK: return 2;
    default: return -1;
  }
}

void espble_hci_controller_policy_init(
  espble_hci_controller_policy_t *policy)
{
  if (policy != NULL) memset(policy, 0, sizeof(*policy));
}

void espble_hci_controller_policy_remove_host(
  espble_hci_controller_policy_t *policy, uint8_t host)
{
  if (policy == NULL || host >= ESPBLE_HCI_CONTROLLER_POLICY_HOST_COUNT) return;
  for (size_t i = 0; i < ESPBLE_HCI_CONTROLLER_POLICY_MASK_COUNT; ++i)
  {
    memset(policy->masks[i][host], 0, sizeof(policy->masks[i][host]));
    policy->present[i][host] = false;
  }
}

espble_hci_controller_policy_result_t
espble_hci_controller_policy_rewrite_command(
  espble_hci_controller_policy_t *policy, uint8_t host,
  const uint8_t *packet, size_t length, uint8_t *output, size_t capacity)
{
  if (policy == NULL || host >= ESPBLE_HCI_CONTROLLER_POLICY_HOST_COUNT ||
      packet == NULL || length < 4 || packet[0] != H4_COMMAND)
    return ESPBLE_HCI_CONTROLLER_POLICY_INVALID_PACKET;

  const uint16_t opcode = (uint16_t)packet[1] | ((uint16_t)packet[2] << 8);
  const int index = mask_index(opcode);
  if (index < 0) return ESPBLE_HCI_CONTROLLER_POLICY_PASSTHROUGH;
  if (length != EVENT_MASK_COMMAND_LENGTH ||
      packet[3] != EVENT_MASK_PARAMETER_LENGTH || output == NULL ||
      capacity < length)
    return ESPBLE_HCI_CONTROLLER_POLICY_INVALID_PACKET;

  memcpy(output, packet, length);
  memcpy(policy->masks[index][host], &packet[4], EVENT_MASK_PARAMETER_LENGTH);
  policy->present[index][host] = true;
  memset(&output[4], 0, EVENT_MASK_PARAMETER_LENGTH);
  for (size_t owner = 0; owner < ESPBLE_HCI_CONTROLLER_POLICY_HOST_COUNT; ++owner)
  {
    if (!policy->present[index][owner]) continue;
    for (size_t byte = 0; byte < EVENT_MASK_PARAMETER_LENGTH; ++byte)
      output[4 + byte] |= policy->masks[index][owner][byte];
  }
  return ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN;
}
