#include "EspBleHciAclCredits.h"

#include <string.h>

enum
{
  H4_COMMAND = 0x01,
  H4_EVENT = 0x04,
  EVT_COMMAND_COMPLETE = 0x0e,
  OPCODE_READ_BUFFER_SIZE = 0x1005,
  OPCODE_HOST_BUFFER_SIZE = 0x0c33,
  OPCODE_SET_CONTROLLER_TO_HOST_FLOW_CONTROL = 0x0c31,
  OPCODE_HOST_NUMBER_OF_COMPLETED_PACKETS = 0x0c35,
  FLOW_CONTROL_ACL_ONLY = 0x01,
  HANDLE_MASK = 0x0fff,
};

static uint16_t read_le16(const uint8_t *data)
{
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void write_le16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value & 0xff);
  data[1] = (uint8_t)(value >> 8);
}

static espble_hci_acl_credit_entry_t *find_entry(
  espble_hci_acl_credits_t *credits, uint16_t handle)
{
  for (size_t i = 0; i < ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS; ++i)
  {
    if (credits->entries[i].used && credits->entries[i].handle == handle)
      return &credits->entries[i];
  }
  return NULL;
}

void espble_hci_acl_credits_init(
  espble_hci_acl_credits_t *credits, uint16_t flush_threshold)
{
  if (credits == NULL) return;
  memset(credits, 0, sizeof(*credits));
  credits->flush_threshold = flush_threshold == 0 ? 1 : flush_threshold;
}

bool espble_hci_acl_credits_observe_event(
  espble_hci_acl_credits_t *credits, const uint8_t *packet, size_t length)
{
  if (credits == NULL || packet == NULL || length < 3) return false;
  if (packet[0] != H4_EVENT || packet[1] != EVT_COMMAND_COMPLETE) return false;
  if ((size_t)packet[2] + 3u != length || length < 14) return false;
  if (read_le16(&packet[4]) != OPCODE_READ_BUFFER_SIZE) return false;
  if (packet[6] != 0x00) return false;

  credits->acl_data_length = read_le16(&packet[7]);
  credits->synchronous_data_length = packet[9];
  credits->acl_packet_count = read_le16(&packet[10]);
  credits->synchronous_packet_count = read_le16(&packet[12]);
  // A controller reporting no ACL buffers cannot run this loop at all.
  credits->controller_buffers_known = credits->acl_packet_count > 0 &&
    credits->acl_data_length > 0;
  return credits->controller_buffers_known;
}

bool espble_hci_acl_credits_ready(const espble_hci_acl_credits_t *credits)
{
  return credits != NULL && credits->controller_buffers_known;
}

size_t espble_hci_acl_credits_build_host_buffer_size(
  const espble_hci_acl_credits_t *credits, uint8_t *output, size_t capacity)
{
  if (!espble_hci_acl_credits_ready(credits) || output == NULL || capacity < 11)
    return 0;

  // The broker forwards each packet into a host's receive callback, which
  // copies it before returning, so it can absorb whatever the controller can
  // hold. Advertising the controller's own geometry keeps throughput while
  // still making every buffer accountable.
  output[0] = H4_COMMAND;
  write_le16(&output[1], OPCODE_HOST_BUFFER_SIZE);
  output[3] = 7;
  write_le16(&output[4], credits->acl_data_length);
  output[6] = (uint8_t)(credits->synchronous_data_length > 0xff ? 0xff :
    credits->synchronous_data_length);
  write_le16(&output[7], credits->acl_packet_count);
  write_le16(&output[9], credits->synchronous_packet_count);
  return 11;
}

size_t espble_hci_acl_credits_build_flow_control_enable(
  const espble_hci_acl_credits_t *credits, uint8_t *output, size_t capacity)
{
  if (!espble_hci_acl_credits_ready(credits) || output == NULL || capacity < 5)
    return 0;

  // ACL only. Synchronous data has no host flow control on this controller,
  // and enabling it would require crediting SCO the same way.
  output[0] = H4_COMMAND;
  write_le16(&output[1], OPCODE_SET_CONTROLLER_TO_HOST_FLOW_CONTROL);
  output[3] = 1;
  output[4] = FLOW_CONTROL_ACL_ONLY;
  return 5;
}

void espble_hci_acl_credits_on_delivered(
  espble_hci_acl_credits_t *credits, uint16_t handle)
{
  if (credits == NULL) return;
  handle &= HANDLE_MASK;

  espble_hci_acl_credit_entry_t *entry = find_entry(credits, handle);
  if (entry == NULL)
  {
    for (size_t i = 0; i < ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS; ++i)
    {
      if (credits->entries[i].used) continue;
      entry = &credits->entries[i];
      entry->used = true;
      entry->handle = handle;
      entry->delivered = 0;
      break;
    }
  }
  if (entry == NULL)
  {
    // Losing a credit costs one controller buffer until the next reset; losing
    // track of a live connection's credits would cost every later packet.
    ++credits->dropped_credits;
    return;
  }

  if (entry->delivered == UINT16_MAX || credits->pending_total == UINT16_MAX)
  {
    ++credits->dropped_credits;
    return;
  }
  ++entry->delivered;
  ++credits->pending_total;
}

void espble_hci_acl_credits_forget_handle(
  espble_hci_acl_credits_t *credits, uint16_t handle)
{
  if (credits == NULL) return;
  espble_hci_acl_credit_entry_t *entry = find_entry(credits, handle & HANDLE_MASK);
  if (entry == NULL) return;
  credits->pending_total = (uint16_t)(credits->pending_total - entry->delivered);
  entry->used = false;
  entry->delivered = 0;
}

size_t espble_hci_acl_credits_build_credits(
  espble_hci_acl_credits_t *credits, bool force, uint8_t *output,
  size_t capacity)
{
  if (credits == NULL || output == NULL) return 0;
  if (credits->pending_total == 0) return 0;
  if (!force && credits->pending_total < credits->flush_threshold) return 0;
  if (capacity < 9) return 0;

  size_t used = 5;
  uint8_t handles = 0;
  for (size_t i = 0; i < ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS; ++i)
  {
    espble_hci_acl_credit_entry_t *entry = &credits->entries[i];
    if (!entry->used || entry->delivered == 0) continue;
    if (used + 4u > capacity) break;
    write_le16(&output[used], entry->handle);
    write_le16(&output[used + 2u], entry->delivered);
    used += 4;
    ++handles;
    credits->pending_total =
      (uint16_t)(credits->pending_total - entry->delivered);
    entry->delivered = 0;
  }
  if (handles == 0) return 0;

  output[0] = H4_COMMAND;
  write_le16(&output[1], OPCODE_HOST_NUMBER_OF_COMPLETED_PACKETS);
  output[3] = (uint8_t)(1u + 4u * handles);
  output[4] = handles;
  return used;
}

void espble_hci_acl_credits_restore(
  espble_hci_acl_credits_t *credits, const uint8_t *packet, size_t length)
{
  if (credits == NULL || packet == NULL || length < 9) return;
  if (packet[0] != H4_COMMAND ||
      read_le16(&packet[1]) != OPCODE_HOST_NUMBER_OF_COMPLETED_PACKETS)
    return;
  if ((size_t)packet[3] + 4u != length ||
      (size_t)packet[3] != 1u + 4u * (size_t)packet[4])
    return;

  for (uint8_t i = 0; i < packet[4]; ++i)
  {
    const uint8_t *record = &packet[5u + 4u * i];
    const uint16_t handle = read_le16(record) & HANDLE_MASK;
    const uint16_t count = read_le16(&record[2]);
    for (uint16_t remaining = count; remaining > 0; --remaining)
      espble_hci_acl_credits_on_delivered(credits, handle);
  }
}
