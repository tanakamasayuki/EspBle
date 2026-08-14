#include "EspBleHciControllerPolicy.h"

#include <string.h>

enum
{
  H4_COMMAND = 0x01,
  OPCODE_SET_EVENT_MASK = 0x0c01,
  OPCODE_RESET = 0x0c03,
  OPCODE_SET_CONTROLLER_TO_HOST_FLOW_CONTROL = 0x0c31,
  OPCODE_HOST_BUFFER_SIZE = 0x0c33,
  OPCODE_HOST_NUMBER_COMPLETED_PACKETS = 0x0c35,
  OPCODE_SET_EVENT_MASK_PAGE_2 = 0x0c63,
  OPCODE_LE_SET_EVENT_MASK = 0x2001,
  EVENT_MASK_PARAMETER_LENGTH = 8,
  EVENT_MASK_COMMAND_LENGTH = 12,
};

static bool valid_exact_command(
  const uint8_t *packet, size_t length, uint16_t opcode,
  uint8_t parameter_length)
{
  return packet != NULL && length == (size_t)parameter_length + 4 &&
    packet[0] == H4_COMMAND &&
    packet[1] == (opcode & 0xff) && packet[2] == (opcode >> 8) &&
    packet[3] == parameter_length;
}

espble_hci_command_scope_t espble_hci_controller_policy_classify_opcode(
  uint16_t opcode)
{
  switch (opcode)
  {
    // Read-only local controller information shared by both hosts.
    case 0x1001: // Read Local Version Information
    case 0x1002: // Read Local Supported Commands
    case 0x1003: // Read Local Supported Features
    case 0x1004: // Read Local Extended Features
    case 0x1005: // Read Buffer Size
    case 0x1009: // Read BD_ADDR
      return ESPBLE_HCI_COMMAND_SCOPE_SHARED_READ;

    case OPCODE_SET_EVENT_MASK:
    case OPCODE_SET_EVENT_MASK_PAGE_2:
    case OPCODE_LE_SET_EVENT_MASK:
      return ESPBLE_HCI_COMMAND_SCOPE_CONTROLLER_MERGED;

    case OPCODE_RESET:
    case OPCODE_SET_CONTROLLER_TO_HOST_FLOW_CONTROL:
    case OPCODE_HOST_BUFFER_SIZE:
      return ESPBLE_HCI_COMMAND_SCOPE_CONTROLLER_VIRTUAL;
    case OPCODE_HOST_NUMBER_COMPLETED_PACKETS:
      return ESPBLE_HCI_COMMAND_SCOPE_HOST_CREDIT;

    // LE procedures and LE-local controller state used by vendored NimBLE.
    case 0x2002: // LE Read Buffer Size
    case 0x2003: // LE Read Local Supported Features
    case 0x2005: // LE Set Random Address
    case 0x2006: // LE Set Advertising Parameters
    case 0x2008: // LE Set Advertising Data
    case 0x2009: // LE Set Scan Response Data
    case 0x200a: // LE Set Advertising Enable
    case 0x200b: // LE Set Scan Parameters
    case 0x200c: // LE Set Scan Enable
    case 0x200d: // LE Create Connection
    case 0x2018: // LE Rand
      return ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_RADIO;
    case 0x2016: // LE Read Remote Features
    case 0x2019: // LE Start Encryption
    case 0x201a: // LE Long Term Key Request Reply
    case 0x2022: // LE Set Data Length
    case 0x2030: // LE Read PHY
      return ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_CONNECTION;

    // BR/EDR procedures and local state used by Classic-only Bluedroid.
    case 0x0405: // Create Connection
    case 0x0409: // Accept Connection Request
    case 0x040a: // Reject Connection Request
    // Every pairing reply names a peer address rather than a connection
    // handle, so they belong with the other address-scoped Classic commands.
    // Both answers of each pair occur: the application can refuse a pairing,
    // and legacy PIN pairing is always refused.
    case 0x040b: // Link Key Request Reply
    case 0x040c: // Link Key Request Negative Reply
    case 0x040d: // PIN Code Request Reply
    case 0x040e: // PIN Code Request Negative Reply
    case 0x0419: // Remote Name Request
    case 0x042b: // IO Capability Request Reply
    case 0x042c: // User Confirmation Request Reply
    case 0x042d: // User Confirmation Request Negative Reply
    case 0x042e: // User Passkey Request Reply
    case 0x042f: // User Passkey Request Negative Reply
    case 0x0430: // Remote OOB Data Request Reply
    case 0x0433: // Remote OOB Data Request Negative Reply
    case 0x0434: // IO Capability Request Negative Reply
    case 0x080f: // Write Default Link Policy Settings
    case 0x0c12: // Delete Stored Link Key
    case 0x0c13: // Write Local Name
    case 0x0c14: // Read Local Name
    case 0x0c17: // Read Page Timeout
    case 0x0c18: // Write Page Timeout
    case 0x0c1a: // Write Scan Enable
    case 0x0c1e: // Write Authentication Enable
    case 0x0c24: // Write Class of Device
    case 0x0c25: // Read Voice Setting
    case 0x0c26: // Write Voice Setting
    case 0x0c2e: // Read Synchronous Flow Control Enable
    case 0x0c2f: // Write Synchronous Flow Control Enable
    case 0x0c3a: // Write Current IAC LAP
    case 0x0c43: // Write Inquiry Scan Type
    case 0x0c45: // Write Inquiry Mode
    case 0x0c47: // Write Page Scan Type
    case 0x0c52: // Write Extended Inquiry Response
    case 0x0c56: // Write Simple Pairing Mode
    case 0x0c5b: // Write Default Erroneous Data Reporting
    case 0xfc82: // ESP vendor: set/clear coexistence status (Classic A2DP)
      return ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_RADIO;
    case 0x040f: // Change Connection Packet Type
    case 0x0411: // Authentication Requested
    case 0x0413: // Set Connection Encryption
    case 0x041b: // Read Remote Supported Features
    case 0x041c: // Read Remote Extended Features
    case 0x041f: // Read Clock Offset
    case 0x0428: // Setup Synchronous Connection
    case 0x0429: // Accept Synchronous Connection Request
    case 0x042a: // Reject Synchronous Connection Request
    case 0x0803: // Sniff Mode
    case 0x0804: // Exit Sniff Mode
    case 0x080d: // Write Link Policy Settings
    case 0x0c37: // Write Link Supervision Timeout
      return ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_CONNECTION;

    // These are valid for either LE or BR/EDR and must follow handle ownership.
    case 0x0406: // Disconnect
    case 0x041d: // Read Remote Version Information
    case 0x1405: // Read RSSI
      return ESPBLE_HCI_COMMAND_SCOPE_SHARED_CONNECTION;
    default:
      return ESPBLE_HCI_COMMAND_SCOPE_UNKNOWN;
  }
}

espble_hci_command_authorization_t espble_hci_controller_policy_authorize(
  uint8_t host, const uint8_t *packet, size_t length)
{
  if (host >= ESPBLE_HCI_CONTROLLER_POLICY_HOST_COUNT || packet == NULL ||
      length < 4 || packet[0] != H4_COMMAND ||
      length != (size_t)packet[3] + 4u)
    return ESPBLE_HCI_COMMAND_INVALID_PACKET;

  const uint16_t opcode = (uint16_t)packet[1] | ((uint16_t)packet[2] << 8);
  const espble_hci_command_scope_t scope =
    espble_hci_controller_policy_classify_opcode(opcode);
  if (scope == ESPBLE_HCI_COMMAND_SCOPE_UNKNOWN)
    return ESPBLE_HCI_COMMAND_UNCLASSIFIED;
  if ((scope == ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_RADIO ||
       scope == ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_CONNECTION) && host != 0)
    return ESPBLE_HCI_COMMAND_WRONG_HOST;
  if ((scope == ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_RADIO ||
       scope == ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_CONNECTION) && host != 1)
    return ESPBLE_HCI_COMMAND_WRONG_HOST;
  return ESPBLE_HCI_COMMAND_AUTHORIZED;
}

bool espble_hci_controller_policy_targets_handle(uint16_t opcode)
{
  switch (opcode)
  {
    case 0x0406: // Disconnect
    case 0x040f: // Change Connection Packet Type
    case 0x0411: // Authentication Requested
    case 0x0413: // Set Connection Encryption
    case 0x041b: // Read Remote Supported Features
    case 0x041c: // Read Remote Extended Features
    case 0x041d: // Read Remote Version Information
    case 0x041f: // Read Clock Offset
    case 0x0428: // Setup Synchronous Connection
    case 0x0803: // Sniff Mode
    case 0x0804: // Exit Sniff Mode
    case 0x080d: // Write Link Policy Settings
    case 0x0c37: // Write Link Supervision Timeout
    case 0x1405: // Read RSSI
    case 0x2016: // LE Read Remote Features
    case 0x2019: // LE Start Encryption
    case 0x201a: // LE Long Term Key Request Reply
    case 0x2022: // LE Set Data Length
    case 0x2030: // LE Read PHY
      return true;
    default:
      return false;
  }
}

bool espble_hci_controller_policy_is_reset(
  const uint8_t *packet, size_t length)
{
  return valid_exact_command(packet, length, OPCODE_RESET, 0);
}

espble_hci_controller_policy_virtual_action_t
espble_hci_controller_policy_virtual_action(
  const uint8_t *packet, size_t length)
{
  if (packet == NULL || length < 4 || packet[0] != H4_COMMAND ||
      length != (size_t)packet[3] + 4)
    return ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET;

  const uint16_t opcode = (uint16_t)packet[1] | ((uint16_t)packet[2] << 8);
  switch (opcode)
  {
    case OPCODE_RESET:
      return packet[3] == 0 ? ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_COMPLETE :
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET;
    case OPCODE_SET_CONTROLLER_TO_HOST_FLOW_CONTROL:
      return packet[3] == 1 ? ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_COMPLETE :
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET;
    case OPCODE_HOST_BUFFER_SIZE:
      return packet[3] == 7 ? ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_COMPLETE :
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET;
    case OPCODE_HOST_NUMBER_COMPLETED_PACKETS:
      if (packet[3] < 1 ||
          (size_t)packet[3] != 1u + 4u * (size_t)packet[4])
        return ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET;
      return ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_NO_RESPONSE;
    default:
      return ESPBLE_HCI_CONTROLLER_POLICY_PHYSICAL;
  }
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
